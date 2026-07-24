import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("General")
        icon: "configure"
        source: "configGeneral.qml"
    }
    ConfigCategory {
        name: i18n("Layout")
        icon: "preferences-desktop-display"
        source: "configLayout.qml"
    }
    ConfigCategory {
        name: i18n("Appearance")
        icon: "preferences-desktop-theme"
        source: "configAppearance.qml"
    }
    ConfigCategory {
        name: i18n("Behavior")
        icon: "preferences-system-windows-behavior"
        source: "configBehavior.qml"
    }
    ConfigCategory {
        name: i18n("Animations")
        icon: "preferences-desktop-effects"
        source: "configAnimation.qml"
    }
}
