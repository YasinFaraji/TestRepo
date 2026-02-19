import QtQuick
import QtQuick.Controls
import Test.Bridge

Window {
    id: root
    width: 1100
    height: 800
    visible: true
    title: "GitEase Pro Diff - VS Code Style"
    color: "#1e1e1e"

    // تعریف استایل هاشور به عنوان یک فایل مجزا یا کامپوننت داخلی
    Component {
        id: diagonalHatch
        Canvas {
            anchors.fill: parent
            opacity: 0.2
            onPaint: {
                var ctx = getContext("2d");
                ctx.strokeStyle = "#ffffff";
                ctx.lineWidth = 1;
                ctx.beginPath();
                var step = 10;
                // رسم خطوط مورب معکوس
                for (var i = -height; i < width + height; i += step) {
                    ctx.moveTo(i + height, 0);
                    ctx.lineTo(i, height);
                }
                ctx.stroke();
            }
        }
    }

    ListView {
        id: diffListView
        anchors.fill: parent
        model: DiffBridge.diffData
        clip: true
        // جلوگیری از لرزش هنگام اسکرول
        pixelAligned: true

        delegate: Item {
            width: diffListView.width
            height: 22

            Row {
                anchors.fill: parent

                // --- ستون چپ (Original / Deleted) ---
                Rectangle {
                    width: (parent.width / 2)
                    height: 22
                    // رنگ قرمز برای حذف (2) یا اصلاح (3)
                    color: (modelData.type === 2 || modelData.type === 3) ? "#4b1818" : (modelData.type === 1 ? "#161616" : "transparent")

                    // لودر هاشور برای ستون چپ (وقتی خط فقط در راست اضافه شده)
                    Loader {
                        anchors.fill: parent
                        sourceComponent: modelData.type === 1 ? diagonalHatch : null
                        active: modelData.type === 1
                    }

                    Row {
                        anchors.fill: parent
                        spacing: 8

                        Text {
                            width: 45
                            text: modelData.oldLine !== -1 ? modelData.oldLine : ""
                            color: "#606060"
                            font.family: "Consolas"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: modelData.type !== 1 ? modelData.content : ""
                            color: "#d4d4d4"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                        }
                    }
                }

                // خط عمودی وسط
                Rectangle {
                    width: 1
                    height: 22
                    color: "#2d2d2d"
                }

                // --- ستون راست (Modified / Added) ---
                Rectangle {
                    width: (parent.width / 2) - 1
                    height: 22
                    // رنگ سبز برای اضافه (1) یا اصلاح (3)
                    color: (modelData.type === 1 || modelData.type === 3) ? "#2d4a2d" : (modelData.type === 2 ? "#161616" : "transparent")

                    // لودر هاشور برای ستون راست (وقتی خط فقط در چپ حذف شده)
                    Loader {
                        anchors.fill: parent
                        sourceComponent: modelData.type === 2 ? diagonalHatch : null
                        active: modelData.type === 2
                    }

                    Row {
                        anchors.fill: parent
                        spacing: 8

                        Text {
                            width: 45
                            text: modelData.newLine !== -1 ? modelData.newLine : ""
                            color: "#606060"
                            font.family: "Consolas"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            // در وضعیت اصلاح (3)، محتوای جدید را نشان می‌دهد
                            text: modelData.type === 3 ? modelData.contentNew : (modelData.type !== 2 ? modelData.content : "")
                            color: "#d4d4d4"
                            font.family: "Consolas"
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }
    }
}
