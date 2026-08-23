import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("Panel Studio")
        icon: "configure"
        source: "configGeneral.qml"
    }
    ConfigCategory {
        name: i18n("Behavior")
        icon: "preferences-system-windows-behavior"
        source: "configBehavior.qml"
    }
}
