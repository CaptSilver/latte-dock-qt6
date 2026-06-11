import QtQuick

Item {
    width: 256; height: 256
    ShaderEffect {
        anchors.fill: parent
        // Points at a .qsb that does not exist -> "shader preparation failed".
        fragmentShader: "file:///nonexistent/definitely-missing.frag.qsb"
    }
}
