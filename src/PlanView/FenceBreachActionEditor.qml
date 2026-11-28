import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Breach action and fallback actions of one fence area (MAV_CMD_NAV_SET_FENCE_ACTION).
/// A fallback is offered once the action before it is set; "None" ends the list there.
ColumnLayout {
    id: root

    required property var fence
    required property var controller
    property string namePrefix: "fenceBreachAction"

    spacing: ScreenTools.defaultFontPixelHeight * 0.25

    readonly property var _actions: fence && fence.breachActions ? fence.breachActions : []
    readonly property var _labels: [ qsTr("Breach action"), qsTr("Fallback action"), qsTr("Second fallback") ]

    function _model(level) {
        let model = [ { value: 0, text: level === 0 ? controller.breachActionName(0) : qsTr("None") } ]
        return model.concat(controller.breachActionChoices)
    }

    function _action(level) {
        return level < _actions.length ? _actions[level] : 0
    }

    // -1 when the action is not one the vehicle supports (e.g. a plan made for another vehicle type)
    function _indexOf(model, level) {
        const action = _action(level)
        for (let i = 0; i < model.length; i++) {
            if (model[i].value === action) {
                return i
            }
        }
        return -1
    }

    Repeater {
        model: 3

        RowLayout {
            id: row

            required property int index

            Layout.fillWidth:   true
            spacing:            ScreenTools.defaultFontPixelWidth
            visible:            index <= root._actions.length

            QGCLabel {
                text:                   root._labels[row.index]
                Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 14
            }

            QGCComboBox {
                id:                 combo
                objectName:         root.namePrefix + row.index
                Layout.fillWidth:   true
                textRole:           "text"
                model:              root._model(row.index)
                currentIndex:       root._indexOf(model, row.index)
                alternateText:      currentIndex === -1 ?
                                        qsTr("%1 (not supported)").arg(root.controller.breachActionName(root._action(row.index))) : ""

                onActivated: (comboIndex) => {
                    if (root.fence) {
                        root.fence.setBreachAction(row.index, model[comboIndex].value)
                    }
                    // Activation replaced the binding, and this combo is reused for the next fence selected
                    currentIndex = Qt.binding(() => root._indexOf(model, row.index))
                }
            }
        }
    }
}
