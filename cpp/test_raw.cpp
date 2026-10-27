#include <GLFW/glfw3.h>
#include <iostream>

void cb(GLFWwindow*, double x, double y) {
    std::cout << "got motion: " << x << ", " << y << "\n";
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* win = glfwCreateWindow(300, 200, "Test Raw Motion", nullptr, nullptr);
    if (!win) return 1;
    
    std::cout << "Testing WITH raw mouse motion...\n";
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
    glfwSetCursorPosCallback(win, cb);
    
    // Poll for 1 second
    double t = glfwGetTime();
    while (glfwGetTime() - t < 1.0) {
        glfwPollEvents();
    }
    
    std::cout << "Done test.\n";
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
