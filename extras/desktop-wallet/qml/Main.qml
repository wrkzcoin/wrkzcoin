import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    width: 1200
    height: 820
    visible: true
    title: "Desktop Wallet"

    property string sendTxHash: ""
    property string integratedAddressOut: ""

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8

            Image {
                source: "qrc:/qml/assets/logo-placeholder.svg"
                fillMode: Image.PreserveAspectFit
                sourceSize.width: 28
                sourceSize.height: 28
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
            }

            Label {
                text: "Wallet C API Desktop Wallet"
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Label {
                text: walletBridge.loaded ? "Loaded" : "Not Loaded"
                color: walletBridge.loaded ? "lightgreen" : "orange"
            }

            Label {
                text: walletBridge.busy ? "Busy" : "Idle"
                color: walletBridge.busy ? "orange" : "lightgreen"
            }
        }
    }

    footer: Frame {
        width: parent.width
        ColumnLayout {
            anchors.fill: parent
            spacing: 4
            Label { text: walletBridge.lastMessage; color: "lightgreen" }
            Label { text: walletBridge.lastError; color: "salmon"; wrapMode: Text.Wrap }
        }
    }

    ColumnLayout {
        anchors.fill: parent

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: "Session" }
            TabButton { text: "Dashboard" }
            TabButton { text: "Send" }
            TabButton { text: "Node" }
            TabButton { text: "Transactions" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            Item {
                ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    TextField { id: walletPath; placeholderText: "Wallet file path"; text: "wallet.wallet" }
                    TextField { id: walletPassword; placeholderText: "Wallet password"; echoMode: TextInput.Password }
                    TextField { id: daemonHost; placeholderText: "Daemon host"; text: "127.0.0.1" }
                    SpinBox { id: daemonPort; from: 1; to: 65535; value: 11898 }
                    CheckBox { id: daemonSsl; text: "Daemon SSL"; checked: false }
                    SpinBox { id: syncThreads; from: 1; to: 64; value: 4 }

                    RowLayout {
                        Button {
                            text: "Open"
                            onClicked: walletBridge.openWallet(
                                walletPath.text, walletPassword.text, daemonHost.text, daemonPort.value, daemonSsl.checked, syncThreads.value)
                        }
                        Button {
                            text: "Create"
                            onClicked: walletBridge.createWallet(
                                walletPath.text, walletPassword.text, daemonHost.text, daemonPort.value, daemonSsl.checked, syncThreads.value)
                        }
                        Button {
                            text: "Close"
                            onClicked: walletBridge.closeWallet()
                        }
                        Button {
                            text: "Save"
                            onClicked: walletBridge.saveWallet()
                        }
                        Button {
                            text: "Delete File"
                            onClicked: walletBridge.deleteWalletFile(walletPath.text)
                        }
                    }

                    Label { text: "Restore from mnemonic seed" }
                    TextArea {
                        id: mnemonicSeed
                        placeholderText: "Enter mnemonic seed"
                        wrapMode: TextArea.Wrap
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                    }
                    SpinBox { id: scanHeight; from: 0; to: 2147483647; value: 0 }
                    Button {
                        text: "Restore from Seed"
                        onClicked: walletBridge.restoreFromSeed(
                            mnemonicSeed.text,
                            walletPath.text,
                            walletPassword.text,
                            scanHeight.value,
                            daemonHost.text,
                            daemonPort.value,
                            daemonSsl.checked,
                            syncThreads.value)
                    }
                }
            }
            }

            Item {
                ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    RowLayout {
                        Button { text: "Refresh"; onClicked: walletBridge.refresh() }
                        Label { text: "Daemon Online: " + (walletBridge.daemonOnline ? "Yes" : "No") }
                    }

                    Label { text: "Wallet Height: " + walletBridge.walletHeight }
                    Label { text: "Local Height: " + walletBridge.localHeight }
                    Label { text: "Network Height: " + walletBridge.networkHeight }
                    Label { text: "Unlocked Balance: " + walletBridge.unlockedBalance }
                    Label { text: "Locked Balance: " + walletBridge.lockedBalance }

                    Label { text: "Primary Address" }
                    TextArea {
                        text: walletBridge.primaryAddress
                        readOnly: true
                        wrapMode: TextArea.WrapAnywhere
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                    }

                    Label { text: "Node Info JSON" }
                    TextArea {
                        text: walletBridge.nodeInfoJson
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        Layout.fillWidth: true
                        Layout.preferredHeight: 160
                    }
                }
            }
            }

            Item {
                ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    TextField { id: sendDestination; placeholderText: "Destination address"; Layout.fillWidth: true }
                    TextField { id: sendAmount; placeholderText: "Amount (atomic units)"; text: "1000" }
                    TextField { id: sendPaymentId; placeholderText: "Payment ID (optional)" }
                    CheckBox { id: sendAll; text: "Send all"; checked: false }

                    Button {
                        text: "Send Basic"
                        onClicked: {
                            walletBridge.sendBasic(
                                sendDestination.text,
                                Number(sendAmount.text),
                                sendPaymentId.text,
                                sendAll.checked
                            )
                        }
                    }

                    Label { text: "Sent TX Hash" }
                    TextField {
                        text: sendTxHash
                        readOnly: true
                        Layout.fillWidth: true
                    }

                    Label { text: "Create integrated address" }
                    TextField { id: iaAddress; placeholderText: "Base address"; Layout.fillWidth: true }
                    TextField { id: iaPaymentId; placeholderText: "Payment ID"; Layout.fillWidth: true }
                    Button {
                        text: "Create Integrated Address"
                        onClicked: walletBridge.createIntegratedAddress(iaAddress.text, iaPaymentId.text)
                    }
                    TextArea {
                        text: integratedAddressOut
                        readOnly: true
                        wrapMode: TextArea.WrapAnywhere
                        Layout.fillWidth: true
                        Layout.preferredHeight: 80
                    }
                }
            }
            }

            Item {
                ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    TextField { id: swapHost; placeholderText: "New daemon host"; text: "127.0.0.1" }
                    SpinBox { id: swapPort; from: 1; to: 65535; value: 11898 }
                    CheckBox { id: swapSsl; text: "SSL"; checked: false }

                    Button {
                        text: "Swap Node"
                        onClicked: walletBridge.swapNode(swapHost.text, swapPort.value, swapSsl.checked)
                    }

                    Label { text: "Reset Wallet Sync" }
                    SpinBox { id: resetHeight; from: 0; to: 2147483647; value: 0 }
                    SpinBox { id: resetTimestamp; from: 0; to: 2147483647; value: 0 }
                    Button {
                        text: "Reset"
                        onClicked: walletBridge.resetWallet(resetHeight.value, resetTimestamp.value)
                    }
                }
            }
            }

            Item {
                ScrollView {
                anchors.fill: parent
                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    RowLayout {
                        Button { text: "Refresh TX"; onClicked: walletBridge.refresh() }
                    }

                    TextArea {
                        text: walletBridge.transactionsJson
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        Layout.fillWidth: true
                        Layout.preferredHeight: 600
                    }
                }
            }
            }
        }
    }

    Connections {
        target: walletBridge
        function onSendResult(result) {
            sendTxHash = result
        }
        function onIntegratedAddressReady(address) {
            integratedAddressOut = address
        }
    }
}
