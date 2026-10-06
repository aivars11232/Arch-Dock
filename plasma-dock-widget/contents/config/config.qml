import org.kde.plasma.configuration

// The applet's Configure dialog: the page that opens Panel Studio and the
// native Behavior page. Every other setting is edited in Panel Studio.
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
