#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <iostream>

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* win = glfwCreateWindow(400, 300, "X11 Test", nullptr, nullptr);
    if (!win) return 1;
    
    Display* dpy = glfwGetX11Display();
    Window xwin = glfwGetX11Window(win);
    
    // Create empty X11 pixmap and cursor
    char data[1] = {0};
    Pixmap pixmap = XCreateBitmapFromData(dpy, xwin, data, 1, 1);
    XColor black;
    black.red = black.green = black.blue = 0;
    Cursor emptyCursor = XCreatePixmapCursor(dpy, pixmap, pixmap, &black, &black, 0, 0);
    XDefineCursor(dpy, xwin, emptyCursor);
    XFreePixmap(dpy, pixmap);
    XFlush(dpy);
    
    std::cout << "Native X11 blank cursor assigned to window.\n";
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
