attribute vec2 inPosition;

uniform mediump vec4 inRect;
uniform vec2 screen;

varying vec2 fragPos;

void main() {
    float x = inRect.x + inRect.p * inPosition.x;
    float y = inRect.y + inRect.q * inPosition.y;
    fragPos = inPosition; // vec2(x, y);
    float glx = x / screen.x;
    float gly = y / screen.y;
    gl_Position = vec4((glx * 2.0) - 1.0, ((gly * 2.0) - 1.0) * -1.0, 0.0, 1.0);
}
