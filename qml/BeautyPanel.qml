// Copyright (c) 2026 GX-Swjtu. SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Rectangle {
    id: panel
    required property var settings
    required property var videoBridge
    property bool dense: false
    property int selectedIndex: 0
    readonly property var effects: [
        {key: "smoothing", name: "磨皮", signed: false, heavy: false},
        {key: "whitening", name: "美白", signed: false, heavy: false},
        {key: "temperature", name: "冷暖", signed: true, heavy: false},
        {key: "tint", name: "色调", signed: true, heavy: false},
        {key: "slimming", name: "瘦脸", signed: false, heavy: true},
        {key: "eyes", name: "大眼", signed: false, heavy: true},
        {key: "lipstick", name: "口红", signed: false, heavy: true},
        {key: "blush", name: "腮红", signed: false, heavy: true}
    ]
    readonly property var selectedEffect: effects[selectedIndex]
    signal closeRequested()
    implicitHeight: dense ? 144 : 172
    color: "#172125"

    // This is part of the player layout, with no modal overlay or preview dimming.
    Rectangle { width: parent.width; height: 1; color: "#30413e" }
    ColumnLayout {
        id: body
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.bottom: parent.bottom
        anchors.topMargin: panel.dense ? 4 : 6; anchors.bottomMargin: panel.dense ? 4 : 6
        width: Math.min(760, panel.width - 24)
        spacing: 0
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: panel.dense ? 44 : 48; spacing: 8
            Column {
                Layout.preferredWidth: 46
                Label { text: panel.selectedEffect.name; font.pixelSize: 14; color: "#e3efe9" }
                Label { text: "全画面"; visible: panel.selectedEffect.signed; font.pixelSize: 10; color: "#9aafa7" }
            }
            Slider {
                id: adjustment; objectName: "beautyAdjustmentSlider"
                Layout.fillWidth: true; implicitHeight: 44
                from: panel.selectedEffect.signed ? -1 : 0; to: 1; stepSize: .01
                value: panel.settings.values[panel.selectedEffect.key]
                enabled: panel.settings.enabled
                Accessible.name: panel.selectedEffect.name + "强度"
                onMoved: panel.settings.setValue(panel.selectedEffect.key, value)
            }
            Label {
                text: Math.round(panel.settings.values[panel.selectedEffect.key] * 100)
                Layout.preferredWidth: 30; horizontalAlignment: Text.AlignRight
                color: panel.settings.enabled ? "#a5ead3" : "#80968d"; font.pixelSize: 13
            }
            Switch {
                objectName: "beautyEffectToggle"; text: "开启"
                visible: panel.selectedEffect.heavy; implicitHeight: 44; implicitWidth: 92
                leftPadding: 0; rightPadding: 0; spacing: 6; font.pixelSize: 12
                checked: panel.settings.values[panel.selectedEffect.key] > 0
                enabled: panel.settings.enabled
                Accessible.name: panel.selectedEffect.name + "开关"
                onToggled: panel.settings.setValue(panel.selectedEffect.key, checked ? .4 : 0)
            }
        }
        ListView {
            id: choices; objectName: "beautyOptions"
            Layout.preferredWidth: Math.min(body.width, contentWidth)
            Layout.preferredHeight: panel.dense ? 48 : 52; Layout.alignment: Qt.AlignHCenter
            orientation: ListView.Horizontal; spacing: 6; clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: panel.effects; currentIndex: panel.selectedIndex
            onWidthChanged: Qt.callLater(() => positionViewAtIndex(currentIndex, ListView.Contain))
            onVisibleChanged: if (visible) Qt.callLater(() => positionViewAtIndex(currentIndex, ListView.Contain))
            delegate: AbstractButton {
                id: choice
                required property var modelData
                required property int index
                objectName: modelData.key + "Option"
                width: 68; height: choices.height - 4
                checkable: true; checked: panel.selectedIndex === index
                Accessible.name: modelData.name
                onClicked: panel.selectedIndex = index
                onActiveFocusChanged: if (activeFocus) choices.positionViewAtIndex(index, ListView.Contain)
                background: Rectangle {
                    radius: 10
                    color: choice.checked ? "#2c4c41" : choice.down ? "#263a33" : "transparent"
                    border.width: choice.checked || choice.activeFocus ? 1 : 0
                    border.color: "#88cdb4"
                }
                contentItem: Column {
                    spacing: 2
                    topPadding: panel.dense ? 4 : 6
                    Label { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: choice.modelData.name; color: choice.checked ? "#c4f7e2" : "#d0ded8"; font.pixelSize: 13 }
                    Label {
                        width: parent.width; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 10
                        color: choice.checked ? "#a5d9c5" : "#8fa89d"
                        text: choice.modelData.heavy && panel.settings.values[choice.modelData.key] === 0
                            ? "关闭" : Math.round(panel.settings.values[choice.modelData.key] * 100)
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.preferredHeight: 44; spacing: 2
            Switch {
                objectName: "beautyMasterToggle"; text: "美颜"; implicitWidth: 98; implicitHeight: 44
                leftPadding: 0; rightPadding: 0; spacing: 6; font.pixelSize: 12
                checked: panel.settings.enabled; onToggled: panel.settings.enabled = checked
            }
            ToolButton {
                objectName: "beautyReset"; text: "恢复推荐"; font.pixelSize: 12; implicitWidth: 68; implicitHeight: 44
                onClicked: panel.settings.reset()
            }
            ToolButton {
                objectName: "beautyCompare"; text: down ? "原图中" : "按住对比"; font.pixelSize: 12; implicitWidth: 76; implicitHeight: 44
                onPressed: panel.videoBridge.compareOriginal = true
                onReleased: panel.videoBridge.compareOriginal = false
                onCanceled: panel.videoBridge.compareOriginal = false
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                objectName: "beautyDone"; text: "完成"; font.pixelSize: 13; implicitWidth: 52; implicitHeight: 44
                Material.foreground: "#a5ead3"
                onClicked: panel.closeRequested()
            }
        }
        Label {
            Layout.fillWidth: true; Layout.preferredHeight: 16; visible: !panel.dense
            text: panel.videoBridge.status
            elide: Text.ElideRight; font.pixelSize: 10; color: "#95aaa2"
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
