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

    // One browser: "<name>  [Launch] [Copy]" + the exact command line (selectable, Ctrl+C works).
    component CommandBlock: ColumnLayout {
        id: block
        property var proxy        // ProxyInstance
        property string title
        property int kind         // 0 Chrome, 1 Edge, 2 Firefox, 3 Yandex Browser
        // proxy.address is a notifying property: both re-evaluate when type/port change.
        readonly property string command: (block.proxy && block.proxy.address) ? block.proxy.browserCommand(block.kind) : ""
        readonly property bool installed: (block.proxy && block.proxy.address) ? block.proxy.browserInstalled(block.kind) : false

        Layout.fillWidth: true
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 8

            ListItemTitleType {
                Layout.fillWidth: true
                text: block.title + (block.installed ? "" : "  · " + qsTr("not found"))
                color: block.installed ? AmneziaStyle.color.paleGray : AmneziaStyle.color.mutedGray
            }

            BasicButtonType {
                Layout.preferredWidth: 110
                Layout.preferredHeight: 40
                text: qsTr("Launch")
                enabled: block.installed

                clickedFunc: function() {
                    if (!block.proxy.running) {
                        PageController.showNotificationMessage(qsTr("Enable the proxy first"))
                        return
                    }
                    if (block.proxy.launch(block.kind)) {
                        PageController.showNotificationMessage(block.title + " " + qsTr("launched via proxy"))
                    } else {
                        PageController.showNotificationMessage(qsTr("Could not launch") + " " + block.title)
                    }
                }
            }

            BasicButtonType {
                Layout.preferredWidth: 90
                Layout.preferredHeight: 40
                text: qsTr("Copy")
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1

                clickedFunc: function() {
                    GC.copyToClipBoard(block.command)
                    PageController.showNotificationMessage(qsTr("Command copied"))
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 6
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 4
            implicitHeight: cmdText.implicitHeight + 24
            radius: 8
            color: AmneziaStyle.color.onyxBlack
            border.width: 1
            border.color: AmneziaStyle.color.slateGray

            TextEdit {
                id: cmdText
                anchors.fill: parent
                anchors.margins: 12
                readOnly: true
                selectByMouse: true
                selectByKeyboard: true
                wrapMode: TextEdit.WrapAnywhere
                textFormat: TextEdit.PlainText
                font.family: "Courier New"
                font.pixelSize: 13
                color: AmneziaStyle.color.paleGray
                selectionColor: AmneziaStyle.color.mutedGray
                selectedTextColor: AmneziaStyle.color.onyxBlack
                text: block.command
            }
        }
    }

    // One full settings block for a single ProxyInstance (direct or vpn).
    component ProxySection: ColumnLayout {
        id: section
        property var proxy        // ProxyInstance (DirectProxyController.direct / .vpn)
        property string title
        property string subtitle

        Layout.fillWidth: true
        spacing: 0

        HeaderTypeWithSwitcher {
            Layout.fillWidth: true
            Layout.topMargin: 32
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: section.title
            descriptionText: section.subtitle

            showSwitcher: true
            switcher {
                checked: section.proxy.enabled
            }
            switcherFunction: function(checked) {
                section.proxy.enabled = checked
            }
        }

        //
        // Proxy type
        //
        VerticalRadioButton {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            checked: section.proxy.proxyType === 0
            text: qsTr("SOCKS5 (with remote DNS)")
            descriptionText: qsTr("Recommended. Universal, resolves DNS on the proxy side")

            onClicked: function() { section.proxy.proxyType = 0 }
            Keys.onEnterPressed: this.clicked()
            Keys.onReturnPressed: this.clicked()
        }

        DividerType {}

        VerticalRadioButton {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            checked: section.proxy.proxyType === 1
            text: qsTr("HTTP (CONNECT)")
            descriptionText: qsTr("Simple, works for browsers")

            onClicked: function() { section.proxy.proxyType = 1 }
            Keys.onEnterPressed: this.clicked()
            Keys.onReturnPressed: this.clicked()
        }

        DividerType {}

        VerticalRadioButton {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            checked: section.proxy.proxyType === 2
            text: qsTr("HTTPS (TLS-encrypted)")
            descriptionText: qsTr("The client→proxy hop is encrypted. Uses a self-signed certificate you trust once on each client")

            onClicked: function() { section.proxy.proxyType = 2 }
            Keys.onEnterPressed: this.clicked()
            Keys.onReturnPressed: this.clicked()
        }

        DividerType {}

        // TLS certificate (only for the HTTPS type)
        LabelWithButtonType {
            visible: section.proxy.proxyType === 2
            Layout.fillWidth: true

            text: qsTr("TLS certificate")
            descriptionText: qsTr("Self-signed, generated on first start. Export it and install into \"Trusted Root\" on each client machine")
            rightImageSource: "qrc:/images/controls/copy.svg"

            clickedFunction: function() {
                var p = section.proxy.exportCertificate()
                if (p !== "") {
                    PageController.showNotificationMessage(qsTr("Certificate exported to Desktop"))
                } else {
                    PageController.showNotificationMessage(qsTr("Start the HTTPS proxy once to generate the certificate"))
                }
            }
        }

        DividerType {
            visible: section.proxy.proxyType === 2
        }

        //
        // Bind address + port
        //
        TextFieldWithHeaderType {
            id: hostField
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Bind address")
            subtitleText: qsTr("127.0.0.1 = this PC only · 0.0.0.0 = all interfaces (other computers) · or a specific IP")
            textField.text: section.proxy.host
            textFieldEditable: true
            textField.onEditingFinished: {
                var h = textField.text.trim()
                if (h === "") { h = "127.0.0.1" }
                section.proxy.host = h
                textField.text = section.proxy.host
            }
        }

        TextFieldWithHeaderType {
            id: portField
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Port")
            textField.text: section.proxy.port
            textField.validator: IntValidator { bottom: 1; top: 65535 }
            textFieldEditable: true
            textField.onEditingFinished: {
                var p = parseInt(textField.text)
                if (isNaN(p) || p < 1 || p > 65535) { p = section.proxy.port }
                section.proxy.port = p
                textField.text = section.proxy.port
            }
        }

        //
        // Authentication (optional)
        //
        SwitcherType {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            text: qsTr("Require login / password")
            descriptionText: qsTr("Off = anonymous proxy. On = clients must authenticate")

            checked: section.proxy.authEnabled
            onToggled: function() {
                if (checked !== section.proxy.authEnabled) {
                    section.proxy.authEnabled = checked
                }
            }
        }

        TextFieldWithHeaderType {
            visible: section.proxy.authEnabled
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Username")
            textField.text: section.proxy.username
            textFieldEditable: true
            textField.onEditingFinished: { section.proxy.username = textField.text.trim() }
        }

        TextFieldWithHeaderType {
            visible: section.proxy.authEnabled
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Password")
            textField.text: section.proxy.password
            textField.echoMode: TextInput.Password
            textFieldEditable: true
            textField.onEditingFinished: { section.proxy.password = textField.text }
        }

        //
        // Allowed client IPs
        //
        TextFieldWithHeaderType {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            headerText: qsTr("Allowed client IPs")
            subtitleText: qsTr("Comma-separated. Empty = allow any client")
            textField.placeholderText: qsTr("e.g. 192.168.1.10, 192.168.1.20")
            textField.text: section.proxy.allowList
            textFieldEditable: true
            textField.onEditingFinished: { section.proxy.allowList = textField.text.trim() }
        }

        //
        // Address / status / copy
        //
        LabelWithButtonType {
            Layout.fillWidth: true
            Layout.topMargin: 16

            text: qsTr("Proxy address")
            descriptionText: section.proxy.address + "  •  " +
                             (section.proxy.running ? qsTr("running") : qsTr("stopped"))
            rightImageSource: "qrc:/images/controls/copy.svg"

            clickedFunction: function() {
                GC.copyToClipBoard(section.proxy.address)
                PageController.showNotificationMessage(qsTr("Address copied"))
            }
        }

        DividerType {}

        //
        // Browsers via this proxy: Launch with one click, or copy the exact command line
        //
        CaptionTextType {
            visible: !section.proxy.viaVpn
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            color: AmneziaStyle.color.mutedGray
            text: qsTr("Browsers via this proxy — Launch opens a separate profile; the line below is the same command in PowerShell syntax (select + Ctrl+C or Copy)")
        }

        CommandBlock { visible: !section.proxy.viaVpn; proxy: section.proxy; title: "Google Chrome";   kind: 0 }
        CommandBlock { visible: !section.proxy.viaVpn; proxy: section.proxy; title: "Microsoft Edge";  kind: 1 }
        CommandBlock { visible: !section.proxy.viaVpn; proxy: section.proxy; title: "Firefox";         kind: 2 }
        CommandBlock { visible: !section.proxy.viaVpn; proxy: section.proxy; title: "Yandex Browser";  kind: 3 }

        //
        // Log
        //
        SwitcherType {
            Layout.fillWidth: true
            Layout.topMargin: 16
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            text: qsTr("Log requested hosts")
            descriptionText: qsTr("Writes every destination host to a file")

            checked: section.proxy.logEnabled
            onToggled: function() {
                if (checked !== section.proxy.logEnabled) {
                    section.proxy.logEnabled = checked
                }
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
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1
                text: qsTr("Open log")
                clickedFunc: function() { section.proxy.openLogFile() }
            }

            BasicButtonType {
                Layout.fillWidth: true
                defaultColor: AmneziaStyle.color.transparent
                hoveredColor: AmneziaStyle.color.translucentWhite
                pressedColor: AmneziaStyle.color.sheerWhite
                textColor: AmneziaStyle.color.paleGray
                borderWidth: 1
                text: qsTr("Clear log")
                clickedFunc: function() {
                    section.proxy.clearLog()
                    PageController.showNotificationMessage(qsTr("Log cleared"))
                }
            }
        }
    }

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
            spacing: 0

            BaseHeaderType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                headerText: qsTr("Local proxies")
                descriptionText: qsTr("Two independent proxies. Each can bind to this PC only or to the network, and can be anonymous or require a login.")
            }

            ProxySection {
                proxy: DirectProxyController.direct
                title: qsTr("Direct proxy — bypasses the VPN")
                subtitle: qsTr("Traffic leaves with your REAL IP while the VPN stays on. For region-locked sites.")
            }

            ProxySection {
                proxy: DirectProxyController.vpn
                title: qsTr("VPN proxy — through the tunnel")
                subtitle: qsTr("Traffic goes out via the VPN. Point another computer at it to share your VPN exit.")
            }

            ParagraphTextType {
                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.bottomMargin: 32
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                color: AmneziaStyle.color.mutedGray
                text: qsTr("Security: a proxy bound to 0.0.0.0 is reachable by anyone who can reach this PC. Use \"Require login / password\" and/or \"Allowed client IPs\" when exposing it to the network.")
            }
        }
    }
}
