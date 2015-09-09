/*
 * Copyright 2015 Canonical Ltd.
 *
 * This file is part of webbrowser-app.
 *
 * webbrowser-app is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * webbrowser-app is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import Qt.labs.settings 1.0
import Ubuntu.Components 1.3
import Ubuntu.Components.Popups 1.0
import Ubuntu.Thumbnailer 0.1
import Ubuntu.Content 1.0
import Ubuntu.Web 0.2
import webbrowserapp.private 0.1
import webbrowsercommon.private 0.1

import "../MimeTypeMapper.js" as MimeTypeMapper

Item {
    id: downloadsItem

    property QtObject downloadsModel

    // We can get file picking requests either via content-hub (activeTransfer)
    // Or via the internal oxide file picker (internalFilePicker) in the case
    // where the user wishes to upload a file from their previous downloads.
    property var activeTransfer
    property var internalFilePicker

    property bool selectMode
    property bool multiSelect
    property alias mimetypeFilter: downloadsMimetypeModel.mimetype

    signal done()

    Rectangle {
        anchors.fill: parent
        color: "#f6f6f6"
    }

    BrowserPageHeader {
        id: title
        text: i18n.tr("Downloads")
        selectButtonVisible: downloadsItem.selectMode
        selectButtonEnabled: downloadsListView.ViewItems.selectedIndices.length > 0
        onBack: {
            if (activeTransfer) {
                activeTransfer.state = ContentTransfer.Aborted
            }
            if (internalFilePicker) {
                internalFilePicker.reject()
            }
            downloadsItem.done()
        }
        onConfirmSelection: {
            var results = []
            if (internalFilePicker) {
                for (var i = 0; i < downloadsListView.ViewItems.selectedIndices.length; i++) {
                    var selectedDownload = downloadsListView.model.get(downloadsListView.ViewItems.selectedIndices[i])
                    results.push(selectedDownload.path)
                }
                internalFilePicker.accept(results)
            } else {
                for (var i = 0; i < downloadsListView.ViewItems.selectedIndices.length; i++) {
                    var selectedDownload = downloadsListView.model.get(downloadsListView.ViewItems.selectedIndices[i])
                    results.push(resultComponent.createObject(downloadsItem, {"url": "file://" + selectedDownload.path}))
                }
                activeTransfer.items = results
                activeTransfer.state = ContentTransfer.Charged
            }
            downloadsItem.done()
        }
    }

    Component {
        id: resultComponent
        ContentItem { }
    }

    ListView {
        id: downloadsListView
        clip: true

        anchors {
            top: title.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            rightMargin: units.gu(2)
        }

        model: DownloadsMimetypeModel {
            id: downloadsMimetypeModel
            sourceModel: downloadsModel
        }

        delegate: DownloadDelegate {
            title: model.filename
            url: model.url
            image: model.mimetype.indexOf("image") === 0 || model.mimetype.indexOf("video") === 0 ? "image://thumbnailer/file://" + model.path : ""
            extension: MimeDatabase.iconForMimetype(model.mimetype) === "-x-generic" ? model.extension : ""
            icon: MimeDatabase.iconForMimetype(model.mimetype) !== "-x-generic" ? MimeDatabase.iconForMimetype(model.mimetype) : ""
            incomplete: !model.complete
            selectMode: downloadsItem.selectMode
            // Work around bug #1493880
            property bool lastSelected

            onSelectedChanged: {
                If (!multiSelect && selected && lastSelected != selected) {
                    downloadsListView.ViewItems.selectedIndices = [index]
                }
                lastSelected = selected
            }

            onClicked: {
                if (model.complete) {
                    if (selectMode) {
                        selected = !selected
                    } else {
                        exportPeerPicker.contentType = MimeTypeMapper.mimeTypeToContentType(model.mimetype)
                        exportPeerPicker.visible = true
                        exportPeerPicker.path = model.path
                    }
                }
            }

            onRemoved: {
                if (model.complete) {
                    downloadsModel.deleteDownload(model.path)
                }
            }
        }

    }

    Component {
        id: contentItemComponent
        ContentItem {}
    }

    ContentPeerPicker {
        id: exportPeerPicker
        visible: false
        anchors.fill: parent
        handler: ContentHandler.Destination
        property string path
        onPeerSelected: {
            var transfer = peer.request()
            if (transfer.state === ContentTransfer.InProgress) {
                transfer.items = [contentItemComponent.createObject(downloadsItem, {"url": path})]
                transfer.state = ContentTransfer.Charged
            }
            visible = false
        }
        onCancelPressed: {
            visible = false
        }
    }

}
