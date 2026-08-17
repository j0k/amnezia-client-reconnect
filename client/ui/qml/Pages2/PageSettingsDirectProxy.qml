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

    BackButtonType {
        id: backButton
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 20 + PageController.safeAreaTopMargin
    }

    FlickableType {
        id: fl
        anchors.top: backButton.bottom
        anchors.bottom: parent.bottom
        contentHeight: content.height

        ColumnLayout {
            id: content
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right

            HeaderTypeWithSwitcher {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Direct proxy")
                descriptionText: qsTr("A local proxy whose traffic bypasses the VPN. Point a browser at it to reach region-locked sites with your real IP while the VPN stays on.")

                showSwitcher: true
                switcher {
                    checked: DirectProxyController.enabled
                }
                switcherFunction: function(checked) {
                    DirectProxyController.enabled = checked
                }
            }

            //
            // Proxy type
            //
            Header2Type {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Proxy type")
            }

            VerticalRadioButton {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                checked: DirectProxyController.proxyType === 0
                text: qsTr("SOCKS5 (with remote DNS)")
                descriptionText: qsTr("Recommended. Universal, resolves DNS directly — best for region unlocks")

                onClicked: function() {
                    DirectProxyController.proxyType = 0
                }
                Keys.onEnterPressed: this.clicked()
                Keys.onReturnPressed: this.clicked()
            }

            DividerType {}

            VerticalRadioButton {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                checked: DirectProxyController.proxyType === 1
                text: qsTr("HTTP (CONNECT)")
                descriptionText: qsTr("Simple, works for browsers")

                onClicked: function() {
                    DirectProxyController.proxyType = 1
                }
                Keys.onEnterPressed: this.clicked()
                Keys.onReturnPressed: this.clicked()
            }

            //
            // Port
            //
            TextFieldWithHeaderType {
                id: portField
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Local port")
                textField.text: DirectProxyController.port
                textField.validator: IntValidator { bottom: 1; top: 65535 }
                textFieldEditable: true
            }

            BasicButtonType {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("Save port")

                clickedFunc: function() {
                    var p = parseInt(portField.textField.text)
                    if (isNaN(p) || p < 1 || p > 65535) { p = 8899 }
                    DirectProxyController.port = p
                    portField.textField.text = DirectProxyController.port
                    PageController.showNotificationMessage(qsTr("Port saved"))
                }
            }

            //
            // Address
            //
            LabelWithButtonType {
                Layout.fillWidth: true
                Layout.topMargin: 24

                text: qsTr("Proxy address")
                descriptionText: DirectProxyController.address + (DirectProxyController.running ? "  •  " + qsTr("running") : "  •  " + qsTr("stopped"))
                rightImageSource: "qrc:/images/controls/copy.svg"

                clickedFunction: function() {
                    GC.copyToClipBoard(DirectProxyController.address)
                    PageController.showNotificationMessage(qsTr("Address copied"))
                }
            }

            DividerType {}

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                color: AmneziaStyle.color.mutedGray
                text: qsTr("Point your browser at this proxy. Example — launch a separate Chrome window that routes through it (PowerShell):")
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 8
                Layout.preferredHeight: cmdText.implicitHeight + 24
                radius: 8
                color: AmneziaStyle.color.onyxBlack
                border.width: 1
                border.color: AmneziaStyle.color.slateGray

                CaptionTextType {
                    id: cmdText
                    anchors.fill: parent
                    anchors.margins: 12
                    font.family: "Courier New"
                    color: AmneziaStyle.color.paleGray
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    text: "& \"C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe\" --proxy-server=" +
                          DirectProxyController.address + " --user-data-dir=$env:TEMP\\chrome-proxy"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8

                BasicButtonType {
                    Layout.fillWidth: true

                    text: qsTr("Launch browser")

                    clickedFunc: function() {
                        if (!DirectProxyController.running) {
                            PageController.showNotificationMessage(qsTr("Enable the proxy first"))
                            return
                        }
                        if (DirectProxyController.launchBrowser()) {
                            PageController.showNotificationMessage(qsTr("Browser launched via proxy"))
                        } else {
                            PageController.showNotificationMessage(qsTr("Could not find Chrome or Edge to launch"))
                        }
                    }
                }

                BasicButtonType {
                    Layout.fillWidth: true

                    defaultColor: AmneziaStyle.color.transparent
                    hoveredColor: AmneziaStyle.color.translucentWhite
                    pressedColor: AmneziaStyle.color.sheerWhite
                    textColor: AmneziaStyle.color.paleGray
                    borderWidth: 1

                    text: qsTr("Copy command")

                    clickedFunc: function() {
                        GC.copyToClipBoard(cmdText.text)
                        PageController.showNotificationMessage(qsTr("Command copied"))
                    }
                }
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                color: AmneziaStyle.color.mutedGray
                text: qsTr("This opens an isolated Chrome whose traffic bypasses the VPN. To route only certain sites through it (rest via VPN), use a PAC file or an extension like SwitchyOmega.")
            }

            //
            // Log
            //
            Header2Type {
                Layout.fillWidth: true
                Layout.topMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Log")
            }

            SwitcherType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                text: qsTr("Log requested hosts")
                descriptionText: qsTr("Writes every destination host to a file — handy to discover a site's domains")

                checked: DirectProxyController.logEnabled
                onToggled: function() {
                    if (checked !== DirectProxyController.logEnabled) {
                        DirectProxyController.logEnabled = checked
                    }
                }
            }

            DividerType {}

            LabelWithButtonType {
                Layout.fillWidth: true

                text: qsTr("Open log file")
                descriptionText: DirectProxyController.logFilePath
                rightImageSource: "qrc:/images/controls/chevron-right.svg"

                clickedFunction: function() {
                    DirectProxyController.openLogFile()
                }
            }

            DividerType {}

            BasicButtonType {
                Layout.fillWidth: true
                Layout.topMargin: 16
                Layout.bottomMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1

                text: qsTr("Clear log")

                clickedFunc: function() {
                    DirectProxyController.clearLog()
                    PageController.showNotificationMessage(qsTr("Log cleared"))
                }
            }
        }
    }
}
