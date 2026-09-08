import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import PageEnum 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

PageType {
    id: root

    // pid -> true for rows the user expanded; survives live model updates (keyed by PID).
    property var expandedPids: ({})

    BackButtonType {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
    }

    ColumnLayout {
        id: header
        anchors.top: backButton.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 0

        BaseHeaderType {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Process recorder")
            descriptionText: qsTr("Records every running process on an interval so you can see what launches — with a timeline to scrub back.")
        }

        //
        // Record button + status
        //
        BasicButtonType {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            defaultColor: ProcessRecorderController.recording ? AmneziaStyle.color.burgundy : AmneziaStyle.color.paleGray
            hoveredColor: ProcessRecorderController.recording ? AmneziaStyle.color.burgundy : AmneziaStyle.color.paleGray
            textColor: ProcessRecorderController.recording ? AmneziaStyle.color.paleGray : AmneziaStyle.color.midnightBlack

            text: ProcessRecorderController.recording ? qsTr("Stop recording") : qsTr("Record apps")

            clickedFunc: function() {
                ProcessRecorderController.toggle()
            }
        }

        CaptionTextType {
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            color: AmneziaStyle.color.mutedGray

            text: {
                if (ProcessRecorderController.snapshotCount === 0)
                    return qsTr("Not recording yet")
                var idx = ProcessRecorderController.viewIndex + 1
                return qsTr("Snapshot %1 / %2  ·  %3  ·  %4 processes  ·  %5 new")
                    .arg(idx)
                    .arg(ProcessRecorderController.snapshotCount)
                    .arg(ProcessRecorderController.viewTimestamp)
                    .arg(ProcessRecorderController.viewProcessCount)
                    .arg(ProcessRecorderController.viewNewCount)
            }
        }

        //
        // Timeline slider
        //
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 12

            Slider {
                id: timeline
                Layout.fillWidth: true
                from: 0
                to: Math.max(0, ProcessRecorderController.snapshotCount - 1)
                value: ProcessRecorderController.viewIndex
                stepSize: 1
                enabled: ProcessRecorderController.snapshotCount > 1

                onMoved: ProcessRecorderController.viewIndex = Math.round(value)
            }

            BasicButtonType {
                implicitHeight: 32
                Layout.preferredWidth: 76
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: ProcessRecorderController.live ? AmneziaStyle.color.goldenApricot : AmneziaStyle.color.paleGray
                borderWidth: 1

                text: ProcessRecorderController.live ? qsTr("● LIVE") : qsTr("Go live")
                enabled: ProcessRecorderController.snapshotCount > 0
                clickedFunc: function() {
                    ProcessRecorderController.jumpToLive()
                }
            }
        }

        //
        // Interval
        //
        TextFieldWithHeaderType {
            id: intervalField
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Snapshot interval, seconds")
            textField.text: (ProcessRecorderController.intervalMs / 1000)
            textField.validator: DoubleValidator { bottom: 0.5; top: 3600; decimals: 2; notation: DoubleValidator.StandardNotation }
            textFieldEditable: true
            textField.onEditingFinished: {
                var s = parseFloat(textField.text.replace(",", "."))
                if (isNaN(s) || s < 0.5) {
                    s = 0.5
                }
                ProcessRecorderController.intervalMs = Math.round(s * 1000)
                textField.text = (ProcessRecorderController.intervalMs / 1000)
            }
        }

        //
        // Filters
        //
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 12

            TextFieldWithHeaderType {
                id: filterField
                Layout.fillWidth: true
                textField.placeholderText: qsTr("Filter by name / path / PID")
                textField.text: ProcessRecorderController.filter
                textFieldEditable: true
                textField.onTextChanged: ProcessRecorderController.filter = textField.text
            }

            SwitcherType {
                Layout.preferredWidth: 140
                text: qsTr("Only new")
                checked: ProcessRecorderController.onlyNew
                onToggled: function() {
                    if (checked !== ProcessRecorderController.onlyNew) {
                        ProcessRecorderController.onlyNew = checked
                    }
                }
            }
        }

        DividerType {}
    }

    //
    // Process list
    //
    ListView {
        id: listView
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.bottomMargin: 8 + PageController.safeAreaBottomMargin

        clip: true
        cacheBuffer: 400
        ScrollBar.vertical: ScrollBarType {}

        model: ProcessRecorderController.processes

        delegate: ColumnLayout {
            id: rowDelegate
            width: listView.width
            spacing: 0

            readonly property bool isExpanded: root.expandedPids[modelData.pid] === true

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: rowContent.implicitHeight + 16

                // Background click target (toggles expand). Sits below the row content, so the
                // real buttons on top (Copy path) still get their own clicks; text/empty areas
                // fall through to here.
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var e = root.expandedPids
                        if (e[modelData.pid] === true) {
                            delete e[modelData.pid]
                        } else {
                            e[modelData.pid] = true
                        }
                        root.expandedPids = e
                    }
                }

                RowLayout {
                    id: rowContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 10

                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 4
                        Layout.preferredWidth: 8
                        Layout.preferredHeight: 8
                        radius: 4
                        color: modelData.exited === true ? AmneziaStyle.color.burgundy : AmneziaStyle.color.goldenApricot
                        visible: modelData.isNew === true || modelData.exited === true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        // Compact line (always visible)
                        CaptionTextType {
                            Layout.fillWidth: true
                            color: modelData.exited === true ? AmneziaStyle.color.burgundy
                                 : (modelData.isNew === true ? AmneziaStyle.color.goldenApricot : AmneziaStyle.color.paleGray)
                            elide: Qt.ElideRight
                            text: (modelData.exited === true ? "EXITED  " : (modelData.isNew === true ? "NEW  " : "")) +
                                  modelData.name + "   ·  PID " + modelData.pid +
                                  (modelData.exited === true
                                       ? ("  ·  работал " + modelData.duration)
                                       : (modelData.startTime !== "" ? ("  ·  запущен " + modelData.startTime) : ""))
                        }

                        // Expanded details (on click)
                        CaptionTextType {
                            visible: rowDelegate.isExpanded
                            Layout.fillWidth: true
                            color: AmneziaStyle.color.mutedGray
                            text: "PID " + modelData.pid + "   ·  PPID " + modelData.ppid +
                                  "   ·  потоков " + modelData.threads +
                                  (modelData.startTime !== "" ? ("   ·  запущен " + modelData.startTime) : "") +
                                  (modelData.exited === true ? ("   ·  работал " + modelData.duration) : "")
                        }

                        CaptionTextType {
                            visible: rowDelegate.isExpanded
                            Layout.fillWidth: true
                            font.family: "Courier New"
                            color: AmneziaStyle.color.paleGray
                            wrapMode: Text.Wrap
                            text: modelData.path !== "" ? modelData.path : qsTr("(path unavailable)")
                        }

                        BasicButtonType {
                            visible: rowDelegate.isExpanded && modelData.path !== ""
                            Layout.topMargin: 4
                            implicitHeight: 28
                            Layout.preferredWidth: 110
                            defaultColor: AmneziaStyle.color.transparent
                            hoveredColor: AmneziaStyle.color.translucentWhite
                            pressedColor: AmneziaStyle.color.sheerWhite
                            textColor: AmneziaStyle.color.paleGray
                            borderWidth: 1
                            text: qsTr("Copy path")
                            clickedFunc: function() {
                                GC.copyToClipBoard(modelData.path)
                                PageController.showNotificationMessage(qsTr("Path copied"))
                            }
                        }
                    }

                    // Expand/collapse hint (plain image; clicks fall through to the MouseArea)
                    Image {
                        Layout.alignment: Qt.AlignVCenter
                        sourceSize.width: 20
                        sourceSize.height: 20
                        source: rowDelegate.isExpanded ? "qrc:/images/controls/chevron-down.svg"
                                                       : "qrc:/images/controls/chevron-right.svg"
                    }
                }
            }

            DividerType {}
        }

        //
        // Footer actions
        //
        footer: ColumnLayout {
            width: listView.width

            BasicButtonType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.bottomMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1

                text: qsTr("Clear history")
                clickedFunc: function() {
                    ProcessRecorderController.clear()
                }
            }
        }
    }
}
