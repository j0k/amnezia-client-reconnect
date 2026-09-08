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

        BasicButtonType {
            visible: !section.proxy.viaVpn
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            text: qsTr("Launch Chrome / Edge via this proxy")

            clickedFunc: function() {
                if (!section.proxy.running) {
                    PageController.showNotificationMessage(qsTr("Enable the proxy first"))
                    return
                }
                if (section.proxy.launchBrowser()) {
                    PageController.showNotificationMessage(qsTr("Browser launched via proxy"))
                } else {
                    PageController.showNotificationMessage(qsTr("Could not find Chrome or Edge"))
                }
            }
        }

        BasicButtonType {
            visible: !section.proxy.viaVpn
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            text: qsTr("Launch Firefox via this proxy")

            clickedFunc: function() {
                if (!section.proxy.running) {
                    PageController.showNotificationMessage(qsTr("Enable the proxy first"))
                    return
                }
                if (section.proxy.launchFirefox()) {
                    PageController.showNotificationMessage(qsTr("Firefox launched via proxy (separate profile)"))
                } else {
                    PageController.showNotificationMessage(qsTr("Could not find Firefox"))
                }
            }
        }

        BasicButtonType {
            visible: !section.proxy.viaVpn
            Layout.fillWidth: true
            Layout.topMargin: 8
            Layout.leftMargin: 16
            Layout.rightMargin: 16

            text: qsTr("Launch Yandex Browser via this proxy")

            clickedFunc: function() {
                if (!section.proxy.running) {
                    PageController.showNotificationMessage(qsTr("Enable the proxy first"))
                    return
                }
                if (section.proxy.launchYandexBrowser()) {
                    PageController.showNotificationMessage(qsTr("Yandex Browser launched via proxy"))
                } else {
                    PageController.showNotificationMessage(qsTr("Could not find Yandex Browser"))
                }
            }
        }

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
