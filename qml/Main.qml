// Qt Multimedia example integration: Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: BSD-3-Clause
// New interface and gestures: Copyright (c) 2026 GX-Swjtu. MIT.
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import QtMultimedia

ApplicationWindow {
    id: root
    width: 1120; height: 760; minimumWidth: android ? 0 : 360; minimumHeight: android ? 0 : 480
    visible: true; title: "映颜 · BeautyPlayer"; color: "#0c1013"
    Material.theme: Material.Dark
    Material.accent: "#9ce6ce"; Material.primary: "#151d22"
    property bool compact: width < 640
    readonly property bool android: Qt.platform.os === "android"
    property bool panelOpen: false
    property bool libraryOpen: true
    property bool controlsVisible: true
    property bool fullScreen: visibility === Window.FullScreen
    property string errorText: ""
    property point lastMousePosition: Qt.point(-1, -1)
    function toggleFullScreen() { fullScreen ? showNormal() : showFullScreen() }
    function timeText(ms) {
        let s = Math.max(0, Math.floor(ms / 1000))
        return (s >= 3600 ? Math.floor(s / 3600) + ":" : "") + Math.floor(s / 60 % 60).toString().padStart(2,"0") + ":" + (s % 60).toString().padStart(2,"0")
    }
    function showControls() { controlsVisible = true; hideTimer.restart() }
    onLibraryOpenChanged: if (libraryOpen) {
        playbackSession.pauseForLibrary()
        panelOpen = false
        hideTimer.stop()
    }
    onClosing: close => {
        if (!android) return
        if (panelOpen) { panelOpen = false; close.accepted = false }
        else if (!libraryOpen) { libraryOpen = true; playbackSession.endHold(); close.accepted = false }
    }
    onPanelOpenChanged: {
        playbackSession.endHold()
        if (!panelOpen) { bridge.compareOriginal = false; showControls() }
    }
    Component.onCompleted: bridge.outputSink = videoOutput.videoSink
    Timer {
        id: hideTimer; objectName: "controlsHideTimer"; interval: root.android ? 4500 : 2500
        onTriggered: {
            if (root.panelOpen || root.libraryOpen) return
            if (seekBar.pressed || gestures.pressed || playbackMenu.opened || optionsDialog.opened || urlDialog.opened) { restart(); return }
            if (!root.android || player.playbackState === MediaPlayer.PlayingState) root.controlsVisible = false
        }
    }
    Connections {
        target: bridge
        function onSourceOpened() { root.libraryOpen = false; root.showControls(); root.errorText = "" }
        function onMediaOpenFailed(message) { root.libraryOpen = false; root.errorText = message; root.showControls() }
    }
    Connections {
        target: player
        function onErrorOccurred(error, errorString) { root.errorText = errorString; root.showControls() }
        function onPlaybackStateChanged() { root.showControls() }
        function onMediaStatusChanged() {
            if (player.mediaStatus === MediaPlayer.LoadedMedia && player.hasAudio && !player.hasVideo)
                root.panelOpen = false
        }
    }
    Connections { target: playbackSession; function onNoticeChanged() { if (playbackSession.notice !== "") { toast.text = playbackSession.notice; toastTimer.restart() } } }
    Shortcut { sequence: "Space"; enabled: !root.libraryOpen && !urlDialog.opened; onActivated: playbackSession.togglePlayback() }
    Shortcut { sequence: "Ctrl+O"; onActivated: fileDialog.open() }
    Shortcut { sequence: "F11"; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "Escape"; onActivated: { if (root.panelOpen) root.panelOpen = false; else if (root.fullScreen) root.showNormal(); else if (root.libraryOpen && player.source.toString() !== "") root.libraryOpen = false } }
    Shortcut { sequence: "Left"; enabled: !root.libraryOpen; onActivated: { bridge.seek(player.position - 10000); root.showControls() } }
    Shortcut { sequence: "Right"; enabled: !root.libraryOpen; onActivated: { bridge.seek(player.position + 10000); root.showControls() } }

    FileDialog {
        id: fileDialog; title: "选择视频或音频"
        nameFilters: ["视频和音频 (*.mp4 *.mkv *.mov *.avi *.webm *.m4v *.ts *.mts *.m2ts *.wmv *.flv *.mpg *.mpeg *.3gp *.opus *.ogg *.oga *.mp3 *.m4a *.aac *.flac *.wav)", "音频 (*.opus *.ogg *.oga *.mp3 *.m4a *.aac *.flac *.wav)", "所有文件 (*)"]
        onAccepted: bridge.open(selectedFile)
    }
    FolderDialog { id: folderDialog; title: "添加视频文件夹"; onAccepted: videoLibrary.addFolder(selectedFolder) }
    Dialog {
        id: urlDialog; title: "播放网址"; anchors.centerIn: parent; width: Math.min(480, root.width - 32)
        modal: true; standardButtons: Dialog.Open | Dialog.Cancel
        TextField { id: urlInput; width: parent.width; placeholderText: "https://…"; selectByMouse: true }
        onAccepted: bridge.open(urlInput.text)
    }

    VideoOutput {
        id: videoOutput; objectName: "videoOutput"; anchors.fill: parent
        // Keep the whole frame visible while tuning, including portrait video.
        anchors.bottomMargin: root.panelOpen && !root.libraryOpen ? beautyPanel.height : 0
        fillMode: VideoOutput.PreserveAspectFit
    }
    Column {
        objectName: "audioPlaceholder"; anchors.centerIn: parent; spacing: 16
        visible: !root.libraryOpen && player.hasAudio && !player.hasVideo
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "♪"; font.pixelSize: 64; color: "#9ce6ce" }
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "音频播放"; font.pixelSize: 20; color: "#c1ccc9" }
    }
    MouseArea {
        id: gestures; objectName: "videoGestures"; anchors.fill: parent
        enabled: !root.libraryOpen && !root.panelOpen
        acceptedButtons: Qt.LeftButton; pressAndHoldInterval: 450
        property real startX: 0
        property real startY: 0
        property real startPosition: 0
        property real preview: 0
        property bool swiping: false
        property bool claimed: false
        onPressed: mouse => { startX = mouse.x; startY = mouse.y; startPosition = player.position; preview = startPosition; swiping = false; claimed = false; hideTimer.stop() }
        onPressAndHold: { if (!swiping) { playbackSession.beginHold(); claimed = playbackSession.holding } }
        onPositionChanged: mouse => {
            let dx = mouse.x - startX, dy = mouse.y - startY
            if (!pressed || !player.seekable) return
            if (!swiping && Math.abs(dx) > 18 && Math.abs(dx) > Math.abs(dy) * 1.2) { swiping = true; claimed = true; playbackSession.endHold() }
            if (swiping) preview = Math.max(0, Math.min(player.duration, startPosition + dx / width * Math.min(120000, player.duration)))
        }
        onReleased: { if (swiping) bridge.seek(preview); playbackSession.endHold(); swiping = false; hideTimer.restart() }
        onCanceled: { playbackSession.endHold(); swiping = false; claimed = true; hideTimer.restart() }
        onClicked: if (!claimed) { root.controlsVisible = !root.controlsVisible; hideTimer.restart() }
        onDoubleClicked: { playbackSession.togglePlayback(); root.showControls() }
    }
    Rectangle {
        anchors.centerIn: parent; width: gestureLabel.implicitWidth + 48; height: 68; radius: 20; color: "#e0202c32"
        visible: gestures.swiping || playbackSession.holding
        Label {
            id: gestureLabel; anchors.centerIn: parent; font.pixelSize: 21; color: "#b7f3df"
            text: playbackSession.holding ? playbackSession.holdSpeed.toFixed(1) + "×  加速播放"
                : root.timeText(gestures.preview) + "   " + (gestures.preview >= gestures.startPosition ? "+" : "−") + Math.round(Math.abs(gestures.preview - gestures.startPosition) / 1000) + " 秒"
        }
    }
    Label {
        anchors.centerIn: parent; width: parent.width - 48; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter
        color: "#ffb4a9"; text: root.errorText; visible: text !== "" && !root.libraryOpen
    }
    Rectangle {
        anchors.top: parent.top; width: parent.width; height: 96
        visible: root.controlsVisible && !root.libraryOpen && !root.panelOpen
        gradient: Gradient { GradientStop { position: 0; color: "#db0b1115" } GradientStop { position: 1; color: "#000b1115" } }
        RowLayout {
            anchors.fill: parent; anchors.margins: 14; anchors.bottomMargin: 26; spacing: 8
            ToolButton { objectName: "backToLibrary"; text: "‹  视频库"; implicitHeight: 48; onClicked: root.libraryOpen = true }
            Label { text: playbackSession.title; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 16; color: "#edf5f3" }
            ToolButton { text: "⋮"; font.pixelSize: 25; implicitWidth: 48; onClicked: playbackMenu.popup() }
        }
    }
    Menu {
        id: playbackMenu
        MenuItem { text: "打开视频"; onTriggered: fileDialog.open() }
        MenuItem { text: "播放网址"; onTriggered: urlDialog.open() }
        MenuItem { text: "播放设置"; onTriggered: optionsDialog.open() }
        MenuItem { text: player.loops === MediaPlayer.Infinite ? "关闭循环" : "循环播放"; onTriggered: player.loops = player.loops === MediaPlayer.Infinite ? 1 : MediaPlayer.Infinite }
    }
    Rectangle {
        objectName: "playbackControls"
        anchors.bottom: parent.bottom; width: parent.width; height: root.compact ? 188 : 164
        visible: root.controlsVisible && !root.libraryOpen && !root.panelOpen
        gradient: Gradient { GradientStop { position: 0; color: "#000b1115" } GradientStop { position: .36; color: "#d80b1115" } GradientStop { position: 1; color: "#f70b1115" } }
        ColumnLayout {
            anchors.fill: parent; anchors.leftMargin: root.compact ? 16 : 32; anchors.rightMargin: root.compact ? 16 : 32; anchors.topMargin: 25; anchors.bottomMargin: 14; spacing: 0
            RowLayout {
                spacing: 12
                Label { text: root.timeText(seekBar.pressed ? seekBar.value : player.position); font.pixelSize: 12; color: "#c1ccc9" }
                Slider {
                    id: seekBar; objectName: "seekBar"; Layout.fillWidth: true; implicitHeight: 44
                    from: 0; to: Math.max(1, player.duration); value: player.position; enabled: player.seekable
                    onPressedChanged: { if (pressed) hideTimer.stop(); else { bridge.seek(value); root.showControls() } }
                }
                Label { text: root.timeText(player.duration); font.pixelSize: 12; color: "#c1ccc9" }
            }
            RowLayout {
                spacing: root.compact ? 2 : 16; Layout.fillWidth: true
                ToolButton { objectName: "beautyEntry"; text: "美颜"; enabled: player.hasVideo; implicitWidth: 60; implicitHeight: 52; onClicked: root.panelOpen = true }
                ToolButton { text: player.playbackRate.toFixed(1) + "×"; implicitWidth: 56; implicitHeight: 52; onClicked: optionsDialog.open() }
                Item { Layout.fillWidth: true }
                ToolButton { text: "−10"; implicitWidth: 48; implicitHeight: 52; onClicked: { bridge.seek(player.position - 10000); root.showControls() } }
                Button {
                    objectName: "playPauseButton"; text: player.playbackState === MediaPlayer.PlayingState ? "Ⅱ" : "▶"; font.pixelSize: 23
                    implicitWidth: 58; implicitHeight: 58; highlighted: true; onClicked: playbackSession.togglePlayback()
                    background: Rectangle { radius: 29; color: parent.down ? "#74bba6" : "#a5ead3" }
                    contentItem: Text { text: parent.text; color: "#092820"; font: parent.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                ToolButton { text: "+10"; implicitWidth: 48; implicitHeight: 52; onClicked: { bridge.seek(player.position + 10000); root.showControls() } }
                Item { Layout.fillWidth: true }
                ToolButton { text: root.fullScreen ? "还原" : "全屏"; implicitWidth: 52; implicitHeight: 52; onClicked: root.toggleFullScreen() }
            }
            Label { text: "长按加速 · 左右滑动快进快退 · 双击播放/暂停"; Layout.alignment: Qt.AlignHCenter; topPadding: 5; font.pixelSize: 11; color: "#889b96"; visible: root.compact }
        }
    }
    Label {
        id: toast; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 90; padding: 14
        visible: toastTimer.running && !root.libraryOpen && !root.panelOpen; color: "#c6f6e6"
        background: Rectangle { radius: 14; color: "#e022302c" }
    }
    Timer { id: toastTimer; interval: 3500 }

    Item {
        anchors.fill: parent; z: 90; visible: !root.android && !root.libraryOpen && !root.panelOpen
        HoverHandler {
            id: desktopHover; acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            blocking: false
            onPointChanged: {
                if (!hovered) return
                const p = point.position
                if (p.x === root.lastMousePosition.x && p.y === root.lastMousePosition.y) return
                root.lastMousePosition = p
                if (p.y >= root.height - (root.compact ? 200 : 176)) root.showControls()
                else if (root.controlsVisible) hideTimer.restart()
            }
        }
    }
    DropArea {
        id: videoDrop; objectName: "videoDropArea"; anchors.fill: parent; z: 100; enabled: !root.android
        onEntered: drag => { drag.accepted = drag.hasUrls && playbackSession.firstDroppedMedia(drag.urls).toString() !== "" }
        onDropped: drop => {
            const video = playbackSession.firstDroppedMedia(drop.urls)
            if (video.toString() !== "") { root.panelOpen = false; bridge.open(video); drop.accept(Qt.CopyAction) }
        }
        Rectangle {
            anchors.fill: parent; visible: videoDrop.containsDrag; color: "#bb101c18"; border.color: "#a5ead3"; border.width: 2
            Label { anchors.centerIn: parent; text: "松开即可播放"; font.pixelSize: 24; color: "#c4f7e2" }
        }
    }

    Rectangle {
        id: libraryPage; anchors.fill: parent; visible: root.libraryOpen; color: "#101619"
        ColumnLayout {
            anchors.fill: parent; anchors.margins: root.compact ? 16 : 32; spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Image { source: "qrc:/branding/mark.png"; Layout.preferredWidth: 46; Layout.preferredHeight: 46 }
                ColumnLayout {
                    spacing: 4; Layout.fillWidth: true
                    Label { text: "映颜"; font.pixelSize: 30; font.bold: true; color: "#cff6e8" }
                    Label { text: "你的视频，自然好看"; color: "#91a7a0"; font.pixelSize: 13 }
                }
                ToolButton { text: "文件"; implicitHeight: 48; onClicked: fileDialog.open() }
                ToolButton { text: "⋮"; font.pixelSize: 24; implicitWidth: 48; onClicked: libraryMenu.popup() }
            }
            TextField { Layout.fillWidth: true; placeholderText: "搜索视频"; implicitHeight: 50; text: videoLibrary.search; onTextEdited: videoLibrary.search = text; selectByMouse: true }
            ListView {
                objectName: "folderShortcuts"
                Layout.fillWidth: true; Layout.preferredHeight: 48; orientation: ListView.Horizontal; spacing: 8; clip: true
                model: videoLibrary.folders
                delegate: Button {
                    required property var modelData
                    objectName: "folderShortcut"
                    readonly property string folderPath: modelData.path
                    text: modelData.name + "  " + modelData.count; highlighted: videoLibrary.folder === modelData.path
                    Material.foreground: highlighted ? "#10372a" : "#dcebe4"
                    contentItem: Text { text: parent.text; font: parent.font; color: parent.highlighted ? "#10372a" : "#dcebe4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: videoLibrary.folder = modelData.path
                    onPressAndHold: if (modelData.path !== "") { folderBlockMenu.path = modelData.path; folderBlockMenu.popup() }
                    ToolTip.visible: hovered; ToolTip.text: modelData.path || "全部视频"
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 4
                ComboBox {
                    objectName: "librarySort"; Layout.fillWidth: true; implicitHeight: 44
                    model: ["文件名", "创建时间", "文件大小"]
                    currentIndex: ["name", "created", "size"].indexOf(videoLibrary.sortBy)
                    onActivated: videoLibrary.sortBy = ["name", "created", "size"][currentIndex]
                    Accessible.name: "视频排序"
                }
                ToolButton { objectName: "librarySortDirection"; text: videoLibrary.descending ? "降序" : "升序"; implicitWidth: 52; implicitHeight: 44; onClicked: videoLibrary.descending = !videoLibrary.descending }
                ToolButton { objectName: "libraryLayoutToggle"; text: videoLibrary.twoColumns ? "双列" : "单列"; implicitWidth: 52; implicitHeight: 44; onClicked: videoLibrary.twoColumns = !videoLibrary.twoColumns }
                ToolButton { text: videoLibrary.scanning ? "扫描中" : "刷新"; enabled: !videoLibrary.scanning; implicitWidth: 60; implicitHeight: 44; onClicked: videoLibrary.refresh() }
            }
            GridView {
                id: videoList; objectName: "videoList"; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                model: videoLibrary; boundsBehavior: Flickable.StopAtBounds; cacheBuffer: 0
                cellWidth: width / (videoLibrary.twoColumns ? 2 : 1)
                cellHeight: videoLibrary.twoColumns ? (cellWidth - 24) * 9 / 16 + 82 : 96
                ScrollBar.vertical: ScrollBar { }
                delegate: ItemDelegate {
                    id: videoTile
                    required property string videoName
                    required property url mediaUrl
                    required property string folderPath
                    required property real sizeMB
                    required property string createdText
                    required property string thumbnailUrl
                    width: videoList.cellWidth - 6; height: videoList.cellHeight - 6; padding: 8
                    background: Rectangle { radius: 12; color: parent.down ? "#2c433b" : parent.hovered ? "#23332e" : "#1a2428" }
                    contentItem: Item {
                        Rectangle {
                            id: previewBox
                            width: videoLibrary.twoColumns ? parent.width : 112
                            height: videoLibrary.twoColumns ? width * 9 / 16 : 68
                            y: videoLibrary.twoColumns ? 0 : (parent.height - height) / 2
                            color: "#24352f"; radius: 6; clip: true
                            Image {
                                id: preview; objectName: "videoThumbnail"
                                anchors.fill: parent; asynchronous: true; cache: true
                                source: libraryPage.visible ? videoTile.thumbnailUrl : ""
                                sourceSize: Qt.size(512, 512); fillMode: Image.PreserveAspectCrop
                            }
                            Image { anchors.centerIn: parent; width: 52; height: 52; source: "qrc:/branding/mark.png"; visible: preview.status !== Image.Ready; opacity: .7 }
                            Label { anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; bottomPadding: 2; text: "暂无预览"; font.pixelSize: 10; color: "#9ab3a6"; visible: preview.status === Image.Error }
                        }
                        Column {
                            x: videoLibrary.twoColumns ? 0 : previewBox.width + 12
                            y: videoLibrary.twoColumns ? previewBox.height + 8 : 9
                            width: parent.width - x; spacing: 5
                            Label { text: videoTile.videoName; width: parent.width; elide: Text.ElideMiddle; color: "#e0ede8"; font.pixelSize: 14 }
                            Label { text: videoTile.sizeMB.toFixed(1) + " MB  ·  " + (videoLibrary.sortBy === "created" ? videoTile.createdText.split(" ")[0] : videoTile.folderPath.split("/").pop()); width: parent.width; elide: Text.ElideRight; font.pixelSize: 11; color: "#9aafa6" }
                        }
                    }
                    Accessible.name: videoName
                    ToolTip.visible: hovered; ToolTip.text: videoName + "\n" + createdText
                    onClicked: bridge.open(mediaUrl)
                }
                Column {
                    anchors.centerIn: parent; width: parent.width; spacing: 16; visible: videoLibrary.count === 0
                    Label { anchors.horizontalCenter: parent.horizontalCenter; text: videoLibrary.scanning ? "正在寻找视频" : "把喜欢的视频放进来"; font.pixelSize: 21; color: "#d0e3da" }
                    Label { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: root.android ? "授权后自动读取手机视频索引\n也可点右上角“文件”选择视频" : "自动查找“视频”和“下载”文件夹\n也可以添加自己的目录"; color: "#839d91"; lineHeight: 1.5; wrapMode: Text.Wrap }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter; text: root.android ? "授权视频访问" : "添加文件夹"; highlighted: true; onClicked: root.android ? videoLibrary.requestAccess() : folderDialog.open()
                        contentItem: Text { text: parent.text; font: parent.font; color: "#10372a"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    }
                }
            }
            Label { text: videoLibrary.status; Layout.fillWidth: true; elide: Text.ElideRight; color: "#708c80"; font.pixelSize: 11 }
            Button { text: "返回播放  ·  " + playbackSession.title; visible: player.source.toString() !== ""; Layout.fillWidth: true; implicitHeight: 52; onClicked: { root.libraryOpen = false; root.showControls() } }
        }
    }
    Menu {
        id: libraryMenu
        MenuItem { text: "添加扫描文件夹"; onTriggered: root.android ? videoLibrary.chooseFolder() : folderDialog.open() }
        MenuItem { text: "授权 / 重新选择视频"; visible: root.android; height: visible ? implicitHeight : 0; onTriggered: videoLibrary.requestAccess() }
        MenuItem { text: "视频库设置"; onTriggered: libraryOptions.open() }
        MenuItem { text: "管理扫描目录"; onTriggered: foldersDialog.open() }
        MenuItem { text: "屏蔽文件夹"; onTriggered: blockedFoldersDialog.open() }
        MenuItem { text: "播放网址"; onTriggered: urlDialog.open() }
        MenuItem { text: "播放设置"; onTriggered: optionsDialog.open() }
    }
    Menu {
        id: folderBlockMenu; property string path: ""
        MenuItem { text: "屏蔽此文件夹及子文件夹"; onTriggered: videoLibrary.setFolderBlocked(folderBlockMenu.path, true) }
    }
    Dialog {
        id: blockedFoldersDialog; objectName: "blockedFoldersDialog"; title: "屏蔽文件夹"; anchors.centerIn: parent
        width: Math.min(550, root.width - 24); modal: true
        footer: DialogButtonBox { Button { text: "关闭"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } onRejected: blockedFoldersDialog.close() }
        contentItem: ColumnLayout {
            spacing: 8
            Label { text: "屏蔽后不出现在视频库中，文件仍保留。\n同时屏蔽子文件夹，可随时恢复。"; Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#abc0b6"; font.pixelSize: 12 }
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: Math.min(360, root.height * .5); clip: true
                model: videoLibrary.folderOptions
                ScrollBar.vertical: ScrollBar { }
                delegate: RowLayout {
                    required property var modelData
                    width: ListView.view.width; height: 68
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 2
                        Label { text: modelData.name; Layout.fillWidth: true; elide: Text.ElideMiddle; color: modelData.blocked ? "#859b91" : "#e0ede8" }
                        Label { text: modelData.path; Layout.fillWidth: true; elide: Text.ElideLeft; font.pixelSize: 10; color: "#82988e" }
                    }
                    Button {
                        text: modelData.explicitBlock ? "恢复" : modelData.blocked ? "随父目录" : "屏蔽"
                        enabled: !modelData.blocked || modelData.explicitBlock
                        implicitHeight: 48; implicitWidth: 76; flat: true
                        Accessible.name: text + " " + modelData.name
                        onClicked: videoLibrary.setFolderBlocked(modelData.path, !modelData.explicitBlock)
                    }
                }
                Label { anchors.centerIn: parent; text: "暂无可管理的文件夹"; visible: parent.count === 0; color: "#95aaa2" }
            }
        }
    }
    Dialog {
        id: libraryOptions; title: "视频库设置"; anchors.centerIn: parent
        width: Math.min(460, root.width - 24); modal: true
        footer: DialogButtonBox { Button { text: "关闭"; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole } onRejected: libraryOptions.close() }
        ColumnLayout {
            width: parent.width; spacing: 10
            Switch { objectName: "respectNoMediaToggle"; text: "识别 .nomedia"; checked: videoLibrary.respectNoMedia; onToggled: videoLibrary.respectNoMedia = checked }
            Label {
                Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 12; color: "#abc0b6"
                text: "开启时跳过含 .nomedia 的文件夹及子文件夹。" + (root.android ? "\n关闭后，需要先添加对应的扫描文件夹并授权，才能查看系统视频库隐藏的视频。" : "")
            }
            Button { text: "添加扫描文件夹"; Layout.fillWidth: true; onClicked: root.android ? videoLibrary.chooseFolder() : folderDialog.open() }
            Label { text: "创建时间缺失时使用入库或修改时间。\n缩略图按需生成并缓存，30 天未使用自动清理；最多保留 256 MB。"; Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 12; color: "#8fa79b" }
        }
    }
    Dialog {
        id: foldersDialog; title: "扫描目录"; anchors.centerIn: parent; width: Math.min(550, root.width - 24); modal: true; standardButtons: Dialog.Close
        contentItem: ListView {
            implicitHeight: Math.min(320, contentHeight); model: videoLibrary.roots; clip: true
            delegate: RowLayout {
                required property string modelData
                width: ListView.view.width; height: 64
                Label { text: videoLibrary.rootLabel(modelData); Layout.fillWidth: true; elide: Text.ElideLeft; font.pixelSize: 12 }
                Button { text: "移除"; flat: true; onClicked: videoLibrary.removeFolder(modelData) }
            }
        }
    }
    BeautyPanel {
        id: beautyPanel; objectName: "beautyPanel"
        anchors.bottom: parent.bottom; width: parent.width; height: implicitHeight
        visible: root.panelOpen && !root.libraryOpen
        dense: root.height < 480
        settings: beautySettings; videoBridge: bridge
        onCloseRequested: root.panelOpen = false
    }
    Dialog {
        id: optionsDialog; anchors.centerIn: parent; title: "播放设置"; width: Math.min(460, root.width - 24); modal: true; standardButtons: Dialog.Close
        ColumnLayout {
            width: parent.width; spacing: 10
            Label { text: "播放速度" }
            ComboBox { Layout.fillWidth: true; model: ["0.5×", "0.75×", "1×", "1.25×", "1.5×", "2×", "3×"]; currentIndex: [0.5,0.75,1,1.25,1.5,2,3].indexOf(playbackSession.normalSpeed); onActivated: playbackSession.normalSpeed = [0.5,0.75,1,1.25,1.5,2,3][currentIndex] }
            Label { text: "长按画面加速" }
            ComboBox { Layout.fillWidth: true; model: ["1.5×", "2×", "3×（默认）", "4×", "5×"]; currentIndex: [1.5,2,3,4,5].indexOf(playbackSession.holdSpeed); onActivated: playbackSession.holdSpeed = [1.5,2,3,4,5][currentIndex] }
            Label { text: "音量" }
            RowLayout { Switch { text: "静音"; checked: audio.muted; onToggled: audio.muted = checked } Slider { Layout.fillWidth: true; from: 0; to: 1; value: audio.volume; onMoved: audio.volume = value } }
            Label { text: "自动记住播放位置\n长按临时加速，松开恢复；左右滑动定位，松开跳转。"; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#a4b6ad"; font.pixelSize: 12 }
            Label { text: bridge.statistics; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#799286"; font.pixelSize: 11 }
        }
    }
}
