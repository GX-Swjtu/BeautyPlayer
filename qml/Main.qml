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
    function toggleFullScreen() { fullScreen ? showNormal() : showFullScreen() }
    function timeText(ms) {
        let s = Math.max(0, Math.floor(ms / 1000))
        return (s >= 3600 ? Math.floor(s / 3600) + ":" : "") + Math.floor(s / 60 % 60).toString().padStart(2,"0") + ":" + (s % 60).toString().padStart(2,"0")
    }
    function showControls() { controlsVisible = true; hideTimer.restart() }
    onClosing: close => {
        if (!android) return
        if (panelOpen) { panelOpen = false; close.accepted = false }
        else if (!libraryOpen) { libraryOpen = true; playbackSession.endHold(); close.accepted = false }
    }
    onPanelOpenChanged: panelOpen ? beautyDrawer.open() : beautyDrawer.close()
    Component.onCompleted: bridge.outputSink = videoOutput.videoSink
    Timer { id: hideTimer; interval: 4500; onTriggered: if (player.playbackState === MediaPlayer.PlayingState && !root.panelOpen && !root.libraryOpen && !seekBar.pressed) root.controlsVisible = false }
    Connections {
        target: bridge
        function onSourceOpened() { root.libraryOpen = false; root.showControls(); root.errorText = "" }
        function onMediaOpenFailed(message) { root.libraryOpen = false; root.errorText = message; root.showControls() }
    }
    Connections {
        target: player
        function onErrorOccurred(error, errorString) { root.errorText = errorString; root.showControls() }
        function onPlaybackStateChanged() { root.showControls() }
    }
    Connections { target: playbackSession; function onNoticeChanged() { if (playbackSession.notice !== "") { toast.text = playbackSession.notice; toastTimer.restart() } } }
    Shortcut { sequence: "Space"; enabled: !urlDialog.opened; onActivated: playbackSession.togglePlayback() }
    Shortcut { sequence: "Ctrl+O"; onActivated: fileDialog.open() }
    Shortcut { sequence: "F11"; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "Escape"; onActivated: { if (root.panelOpen) root.panelOpen = false; else if (root.fullScreen) root.showNormal(); else if (root.libraryOpen && player.source.toString() !== "") root.libraryOpen = false } }
    Shortcut { sequence: "Left"; onActivated: { bridge.seek(player.position - 10000); root.showControls() } }
    Shortcut { sequence: "Right"; onActivated: { bridge.seek(player.position + 10000); root.showControls() } }

    FileDialog {
        id: fileDialog; title: "选择视频"
        nameFilters: ["视频 (*.mp4 *.mkv *.mov *.avi *.webm *.m4v *.ts *.mts *.m2ts *.wmv *.flv *.mpg *.mpeg *.3gp)", "所有文件 (*)"]
        onAccepted: bridge.open(selectedFile)
    }
    FolderDialog { id: folderDialog; title: "添加视频文件夹"; onAccepted: videoLibrary.addFolder(selectedFolder) }
    Dialog {
        id: urlDialog; title: "播放网址"; anchors.centerIn: parent; width: Math.min(480, root.width - 32)
        modal: true; standardButtons: Dialog.Open | Dialog.Cancel
        TextField { id: urlInput; width: parent.width; placeholderText: "https://…"; selectByMouse: true }
        onAccepted: bridge.open(urlInput.text)
    }

    VideoOutput { id: videoOutput; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit }
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
        visible: root.controlsVisible && !root.libraryOpen
        gradient: Gradient { GradientStop { position: 0; color: "#db0b1115" } GradientStop { position: 1; color: "#000b1115" } }
        RowLayout {
            anchors.fill: parent; anchors.margins: 14; anchors.bottomMargin: 26; spacing: 8
            ToolButton { text: "‹  视频库"; implicitHeight: 48; onClicked: { root.libraryOpen = true; playbackSession.endHold() } }
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
        anchors.bottom: parent.bottom; width: parent.width; height: root.compact ? 188 : 164
        visible: root.controlsVisible && !root.libraryOpen
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
                ToolButton { text: "美颜"; implicitWidth: 60; implicitHeight: 52; onClicked: root.panelOpen = true }
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
        visible: toastTimer.running && !root.libraryOpen; color: "#c6f6e6"
        background: Rectangle { radius: 14; color: "#e022302c" }
    }
    Timer { id: toastTimer; interval: 3500 }

    Rectangle {
        id: libraryPage; anchors.fill: parent; visible: root.libraryOpen; color: "#101619"
        ColumnLayout {
            anchors.fill: parent; anchors.margins: root.compact ? 16 : 32; spacing: 12
            RowLayout {
                Layout.fillWidth: true
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
                Layout.fillWidth: true; Layout.preferredHeight: 48; orientation: ListView.Horizontal; spacing: 8; clip: true
                model: videoLibrary.folders
                delegate: Button {
                    required property var modelData
                    text: modelData.name + "  " + modelData.count; highlighted: videoLibrary.folder === modelData.path
                    Material.foreground: highlighted ? "#10372a" : "#dcebe4"
                    contentItem: Text { text: parent.text; font: parent.font; color: parent.highlighted ? "#10372a" : "#dcebe4"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    onClicked: videoLibrary.folder = modelData.path
                    ToolTip.visible: hovered; ToolTip.text: modelData.path || "全部视频"
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label { text: videoLibrary.folder || "最近更新"; elide: Text.ElideLeft; Layout.fillWidth: true; color: "#c4d3ce"; font.pixelSize: 13 }
                ToolButton { text: videoLibrary.scanning ? "扫描中…" : "刷新"; enabled: !videoLibrary.scanning; implicitHeight: 44; onClicked: videoLibrary.refresh() }
            }
            ListView {
                id: videoList; objectName: "videoList"; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6
                model: videoLibrary; reuseItems: true; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { }
                delegate: ItemDelegate {
                    required property string videoName
                    required property url mediaUrl
                    required property string folderPath
                    required property real sizeMB
                    width: videoList.width; height: 82
                    background: Rectangle { radius: 14; color: parent.down ? "#2c433b" : parent.hovered ? "#23332e" : "#1a2428" }
                    contentItem: RowLayout {
                        spacing: 14
                        Rectangle { width: 52; height: 52; radius: 12; color: "#2b413a"; Label { anchors.centerIn: parent; text: "▶"; color: "#ade8d1"; font.pixelSize: 20 } }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 6
                            Label { text: videoName; Layout.fillWidth: true; elide: Text.ElideMiddle; color: "#e0ede8"; font.pixelSize: 15 }
                            Label { text: sizeMB.toFixed(1) + " MB  ·  " + (root.android ? folderPath.split("/").pop() : folderPath); Layout.fillWidth: true; elide: Text.ElideLeft; font.pixelSize: 11; color: "#8fa69d" }
                        }
                    }
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
        MenuItem { text: root.android ? "授权 / 重新选择视频" : "添加文件夹"; onTriggered: root.android ? videoLibrary.requestAccess() : folderDialog.open() }
        MenuItem { text: "管理扫描目录"; visible: !root.android; height: visible ? implicitHeight : 0; onTriggered: foldersDialog.open() }
        MenuItem { text: "播放网址"; onTriggered: urlDialog.open() }
        MenuItem { text: "播放设置"; onTriggered: optionsDialog.open() }
    }
    Dialog {
        id: foldersDialog; title: "扫描目录"; anchors.centerIn: parent; width: Math.min(550, root.width - 24); modal: true; standardButtons: Dialog.Close
        contentItem: ListView {
            implicitHeight: Math.min(320, contentHeight); model: videoLibrary.roots; clip: true
            delegate: RowLayout {
                required property string modelData
                width: ListView.view.width; height: 64
                Label { text: modelData; Layout.fillWidth: true; elide: Text.ElideLeft; font.pixelSize: 12 }
                Button { text: "移除"; flat: true; onClicked: videoLibrary.removeFolder(modelData) }
            }
        }
    }
    Drawer {
        id: beautyDrawer; edge: Qt.BottomEdge; width: root.width; height: Math.min(root.height * .83, 650)
        Overlay.modal: Rectangle { color: "#99060d0a" }
        modal: true; interactive: true; onClosed: { root.panelOpen = false; root.showControls(); bridge.compareOriginal = false }
        background: Rectangle { color: "#192428"; radius: 24 }
        ColumnLayout {
            anchors.fill: parent; anchors.margins: root.compact ? 18 : 28; spacing: 6
            Rectangle { Layout.alignment: Qt.AlignHCenter; width: 40; height: 4; radius: 2; color: "#4f665c" }
            RowLayout {
                Label { text: "自然好看"; font.pixelSize: 23; font.bold: true; Layout.fillWidth: true }
                Switch { text: "美颜"; checked: beautySettings.enabled; onToggled: beautySettings.enabled = checked }
                ToolButton { text: "完成"; implicitHeight: 48; onClicked: root.panelOpen = false }
            }
            TabBar {
                id: beautyTabs; Layout.fillWidth: true
                TabButton { text: "肤质与色彩"; implicitHeight: 48 }
                TabButton { text: "五官与美妆"; implicitHeight: 48 }
            }
            Label { text: beautyTabs.currentIndex === 0 ? "已为你调好 · 冷暖与色调作用于全画面" : "按需开启 · 关闭时不进行人脸检测"; color: "#94ada1"; font.pixelSize: 12; topPadding: 6; bottomPadding: 4 }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
                GridLayout {
                    width: parent.width; columns: root.compact ? 1 : 2; columnSpacing: 28; rowSpacing: 4
                    Repeater {
                        model: beautyTabs.currentIndex === 0 ? [
                            {key:"smoothing",name:"磨皮",negative:false,heavy:false}, {key:"whitening",name:"美白",negative:false,heavy:false},
                            {key:"temperature",name:"冷暖",negative:true,heavy:false}, {key:"tint",name:"色调 · 绿 / 红",negative:true,heavy:false}
                        ] : [
                            {key:"slimming",name:"瘦脸",negative:false,heavy:true}, {key:"eyes",name:"大眼",negative:false,heavy:true},
                            {key:"lipstick",name:"口红",negative:false,heavy:true}, {key:"blush",name:"腮红",negative:false,heavy:true}
                        ]
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true; spacing: 0
                            RowLayout {
                                Layout.fillWidth: true; Layout.preferredHeight: 42
                                Label { text: modelData.name; Layout.fillWidth: true; font.pixelSize: 14 }
                                Label { text: Math.round(beautySettings.values[modelData.key] * 100); color: "#aac8ba" }
                                Switch { visible: modelData.heavy; checked: beautySettings.values[modelData.key] > 0; enabled: beautySettings.enabled; onToggled: beautySettings.setValue(modelData.key, checked ? .4 : 0) }
                            }
                            Slider {
                                objectName: modelData.key + "Slider"; Layout.fillWidth: true; implicitHeight: 44
                                from: modelData.negative ? -1 : 0; to: 1; stepSize: .01; value: beautySettings.values[modelData.key]
                                enabled: beautySettings.enabled && (!modelData.heavy || value > 0)
                                onMoved: beautySettings.setValue(modelData.key, value)
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Button { text: "恢复推荐"; flat: true; implicitHeight: 50; onClicked: beautySettings.reset() }
                Button {
                    text: "按住看原图"; Layout.fillWidth: true; implicitHeight: 50
                    onPressed: bridge.compareOriginal = true
                    onReleased: bridge.compareOriginal = false
                    onCanceled: bridge.compareOriginal = false
                }
            }
            Label { text: bridge.status; color: "#88a99b"; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
        }
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
