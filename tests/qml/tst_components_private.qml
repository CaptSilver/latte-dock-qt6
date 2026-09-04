/*
    SPDX-FileCopyrightText: 2026 Latte Dock contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// The components module installs private/ wholesale, so the only thing that decides whether a file
// in there is alive is whether some sibling still instantiates it. When the mobile text-editing
// leftovers went, the three survivors and the files importing the directory had to keep compiling.
// ItemDelegate is here because it dropped its `import "private"` and nothing else in the suite ever
// compiles it -- ComboBox is covered by tst_rectangularshadow.

import QtQuick
import QtTest

TestCase {
    id: testCase
    name: "ComponentsPrivate"
    when: windowShown
    visible: true
    width: 200
    height: 200

    readonly property string componentsDir: "../../declarativeimports/components/"

    function compileReady(relative) {
        const url = Qt.resolvedUrl(componentsDir + relative);
        const component = Qt.createComponent(url);
        verify(component.status === Component.Ready, relative + " failed to compile: " + component.errorString());
        return component;
    }

    function test_privateComponentsCompile_data() {
        return [
            {tag: "ButtonShadow", file: "private/ButtonShadow.qml"},
            {tag: "RoundShadow", file: "private/RoundShadow.qml"},
            {tag: "TextFieldFocus", file: "private/TextFieldFocus.qml"}
        ];
    }

    function test_privateComponentsCompile(data) {
        compileReady(data.file);
    }

    function test_privateDirectoryConsumersCompile_data() {
        return [
            {tag: "ItemDelegate", file: "ItemDelegate.qml"},
            {tag: "Slider", file: "Slider.qml"}
        ];
    }

    function test_privateDirectoryConsumersCompile(data) {
        compileReady(data.file);
    }
}
