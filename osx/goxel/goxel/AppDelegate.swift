//
//  AppDelegate.swift
//  goxel
//
//  Created by Guillaume Chereau on 10/16/15.
//  Copyright © 2015 Noctua Software Limited. All rights reserved.
//

import Cocoa
import OpenGL
import GLKit
import AppKit
import Foundation

class GoxNSOpenGLView: NSOpenGLView {
    override func awakeFromNib()
    {
        let attr = [
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAOpenGLProfile),
            NSOpenGLPixelFormatAttribute(NSOpenGLProfileVersionLegacy),
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAColorSize), 24,
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAAlphaSize), 8,
            NSOpenGLPixelFormatAttribute(NSOpenGLPFADoubleBuffer),
            NSOpenGLPixelFormatAttribute(NSOpenGLPFADepthSize), 32,
            NSOpenGLPixelFormatAttribute(NSOpenGLPFAStencilSize), 8,
            0
        ]

        self.wantsBestResolutionOpenGLSurface = true
        let format = NSOpenGLPixelFormat(attributes: attr)
        let context = NSOpenGLContext(format: format!, share: nil)
        
        self.openGLContext = context
        self.openGLContext?.makeCurrentContext()
        self.window?.acceptsMouseMovedEvents = true
    }
    
    func appDelegate () -> AppDelegate
    {
        return NSApplication.shared.delegate as! AppDelegate
    }
    
    override var acceptsFirstResponder: Bool {
        return true
    }
    
    func mouseEvent(_ id: Int, _ state: Int, _ event: NSEvent) {
        appDelegate().inputs.touches.0.pos.0 =
            Float(event.locationInWindow.x);
        appDelegate().inputs.touches.0.pos.1 =
            Float(self.frame.height - event.locationInWindow.y);

        // XXX: find a way to make it work with unsage memory.
        switch (id) {
        case 0: appDelegate().inputs.touches.0.down.0 = (state != 0);
        case 1: appDelegate().inputs.touches.0.down.1 = (state != 0);
        case 2: appDelegate().inputs.touches.0.down.2 = (state != 0);
        default: break;
        }
        // Force an update after a mousedown event to make sure that it will
        // be recognised even if we release the mouse immediately.
        if state == 1 {
            goxel_iter(&appDelegate().inputs)
        }
    }
    
    override func mouseDown(with event: NSEvent)         { mouseEvent(0, 1, event) }
    override func mouseUp(with event: NSEvent)           { mouseEvent(0, 0, event) }
    override func mouseDragged(with event: NSEvent)      { mouseEvent(0, 2, event) }
    override func mouseMoved(with event: NSEvent)        { mouseEvent(0, 0, event) }
    override func rightMouseDown(with event: NSEvent)    { mouseEvent(2, 1, event) }
    override func rightMouseUp(with event: NSEvent)      { mouseEvent(2, 0, event) }
    override func rightMouseDragged(with event: NSEvent) { mouseEvent(2, 1, event) }
    override func otherMouseDown(with event: NSEvent)    { mouseEvent(1, 1, event) }
    override func otherMouseUp(with event: NSEvent)      { mouseEvent(1, 0, event) }
    override func otherMouseDragged(with event: NSEvent) { mouseEvent(1, 1, event) }

    override func scrollWheel(with event: NSEvent) {
        appDelegate().inputs.mouse_wheel = Float(event.scrollingDeltaY)
    }
    
    override func keyDown(with event: NSEvent) {
        switch (event.keyCode) {
            case 36: appDelegate().inputs.keys.257 = true
            case 49: appDelegate().inputs.keys.32 = true
            case 51: appDelegate().inputs.keys.259 = true
            case 123: appDelegate().inputs.keys.263 = true
            case 124: appDelegate().inputs.keys.262 = true
            case 125: appDelegate().inputs.keys.264 = true
            case 126: appDelegate().inputs.keys.265 = true
            default: break
        }
        let scalars = event.characters!.unicodeScalars
        let c = scalars[scalars.startIndex].value
        if (c < 256) {
            appDelegate().inputs.chars.0 = c
        }
    }
    
    override func keyUp(with event: NSEvent) {
        switch (event.keyCode) {
            case 36: appDelegate().inputs.keys.257 = false
            case 49: appDelegate().inputs.keys.32 = false
            case 51: appDelegate().inputs.keys.259 = false
            case 123: appDelegate().inputs.keys.263 = false
            case 124: appDelegate().inputs.keys.262 = false
            case 125: appDelegate().inputs.keys.264 = false
            case 126: appDelegate().inputs.keys.265 = false
            default: break
        }
    }
    
    override func flagsChanged(with event: NSEvent) {
        appDelegate().inputs.keys.340 = event.modifierFlags.contains(.shift)
        appDelegate().inputs.keys.341 = event.modifierFlags.contains(.control)
    }
}


@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    @IBOutlet weak var window: NSWindow!
    @IBOutlet weak var view: GoxNSOpenGLView!
    fileprivate var timer: Timer!
    
    open var inputs = inputs_t()
    var userDirectory: [CChar]? = nil;

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Set fullscreen.
        if let screen = NSScreen.main {
            window.setFrame(screen.visibleFrame, display: true, animate: true)
        }
        timer = Timer(timeInterval: 1.0 / 60.0,
            target: self,
            selector: #selector(AppDelegate.onTimer(_:)),
            userInfo: nil,
            repeats: true)
        RunLoop.current.add(timer, forMode: RunLoop.Mode.default)

        sys_callbacks.user = Unmanaged.passUnretained(self).toOpaque()
        sys_callbacks.set_window_title = { (user, title) in
            let mySelf = Unmanaged<AppDelegate>.fromOpaque(user!).takeUnretainedValue()
            let title = String(cString: title!)
            mySelf.window.title = title
        }
        sys_callbacks.get_user_dir = { (user) in
            let mySelf = Unmanaged<AppDelegate>.fromOpaque(user!).takeUnretainedValue()
            if mySelf.userDirectory == nil {
                let paths = NSSearchPathForDirectoriesInDomains(
                    .applicationSupportDirectory, .userDomainMask, true)
                if paths.count > 0 {
                    mySelf.userDirectory = (paths[0] + "/Goxel").cString(using: .utf8);
                }
            }
            return UnsafePointer(mySelf.userDirectory)
        }
        goxel_init()
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }
    
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }
    
    @objc func onTimer(_ sender: Timer!) {
        var r : Int32
        if (!window.isVisible) {
            return
        }
        glClear(GLbitfield(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT))
        self.inputs.window_size = (Int32(self.view.frame.size.width),
                                   Int32(self.view.frame.size.height))
        self.inputs.scale = Float(window.backingScaleFactor)
        r = goxel_iter(&self.inputs)
        goxel_render()
        self.inputs.mouse_wheel = 0
        self.inputs.chars.0 = 0
        glFlush()
        view.openGLContext?.flushBuffer()
        if r == 1 {
            NSApplication.shared.terminate(nil)
        }
    }
}

