precision mediump float;

varying vec2 fragPos;

uniform sampler2D textImg;

void main() {
    gl_FragColor = texture2D(textImg, fragPos);
}
