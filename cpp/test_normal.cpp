#include <GLFW/glfw3.h>
#include <iostream>

void cursor_pos(GLFWwindow*, double x, double y) {
    std::cout << "cursor_pos: " << x << ", " << y << std::endl;
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* win = glfwCreateWindow(300, 200, "Normal Mode", nullptr, nullptr);
    if (!win) return 1;
    glfwSetCursorPosCallback(win, cursor_pos);
    std::cout << "Mode is NORMAL. Please move mouse over window...\n";
    double t = glfwGetTime();
    while (glfwGetTime() - t < 2.0) {
        glfwPollEvents();
    }
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
