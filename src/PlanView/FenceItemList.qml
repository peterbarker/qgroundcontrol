import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// List of the fences and the breach return point of the GeoFence tab
QGCListView {
    id: root

    required property var page

    signal itemActivated

    objectName: "planPanel_fenceList"
    model:      page.entries
    spacing:    ScreenTools.defaultFontPixelHeight * 0.1
    clip:       true

    delegate: PlanPanelRow {
        id: row

        required property var modelData

        readonly property var fence: root.page.objectFor(modelData.kind, modelData.index)

        objectName: "planPanel_fenceRow_" + modelData.key
        width:      root.width
        selected:   modelData.key === root.page.selectedKey

        onClicked: {
            root.page.selectEntry(modelData)
            root.itemActivated()
        }

        QGCColoredImage {
            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
            Layout.preferredWidth:  Layout.preferredHeight
            source:                 row.modelData.kind === "breach" ? "/res/waypoint.svg" : "/res/GeoFence.svg"
            color:                  qgcPal.textPrimary
            fillMode:               Image.PreserveAspectFit
            sourceSize.height:      height
        }

        QGCLabel {
            Layout.fillWidth:       true
            Layout.minimumWidth:    implicitWidth
            elide:                  Text.ElideRight
            text: {
                switch (row.modelData.kind) {
                case "polygon":
                    return qsTr("Polygon %1").arg(row.modelData.index + 1)
                case "circle":
                    return qsTr("Circle %1").arg(row.modelData.index + 1)
                }
                return qsTr("Breach return point")
            }
        }

        QGCLabel {
            objectName:             "planPanel_fenceRowMeta"
            Layout.fillWidth:       true
            Layout.maximumWidth:    implicitWidth
            elide:                  Text.ElideRight
            color:                  qgcPal.textSecondary
            font.pointSize:         ScreenTools.smallFontPointSize
            text: {
                if (row.modelData.kind === "breach") {
                    return root.page.controller.breachReturnPoint.isValid ? qsTr("Set") : qsTr("Not set")
                }
                if (!row.fence) {
                    return ""
                }
                const inclusion = row.fence.inclusion ? qsTr("Include") : qsTr("Exclude")
                let meta = inclusion
                if (row.modelData.kind === "circle") {
                    const kind = row.fence.homeCentered ? qsTr("Home") : inclusion
                    meta = qsTr("%1 %2, %3").arg(row.fence.radius.valueString).arg(row.fence.radius.units).arg(kind)
                }
                if (row.fence.breachActions.length > 0) {
                    meta = qsTr("%1, %2").arg(meta).arg(root.page.controller.breachActionsDescription(row.fence.breachActions))
                }
                return meta
            }
        }
    }

    QGCPalette { id: qgcPal }
}
