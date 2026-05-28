#import <Cocoa/Cocoa.h>

#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/session/ApplicationSession.hpp"
#include "lamusica/session/Project.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

bool hasArgument(int argc, char** argv, std::string_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::string_view{argv[index]} == expected) {
            return true;
        }
    }
    return false;
}

NSMenuItem* addMenuItem(NSMenu* menu, NSString* title, SEL action, NSString* keyEquivalent,
                        id target = nil) {
    auto* item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:keyEquivalent];
    item.target = target;
    [menu addItem:item];
    return item;
}

NSView* makePanel(NSString* label, NSColor* color) {
    auto* view = [[NSView alloc] initWithFrame:NSZeroRect];
    view.wantsLayer = YES;
    view.layer.backgroundColor = color.CGColor;

    auto* text = [NSTextField labelWithString:label];
    text.translatesAutoresizingMaskIntoConstraints = NO;
    text.textColor = NSColor.labelColor;
    text.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold];
    [view addSubview:text];

    [NSLayoutConstraint activateConstraints:@[
        [text.leadingAnchor constraintEqualToAnchor:view.leadingAnchor constant:12.0],
        [text.topAnchor constraintEqualToAnchor:view.topAnchor constant:10.0],
    ]];

    return view;
}

NSWindow* buildPreferencesWindow() {
    const NSRect frame = NSMakeRect(180.0, 180.0, 680.0, 460.0);
    auto* window =
        [[NSWindow alloc] initWithContentRect:frame
                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    window.title = @"LaMusica Preferences";

    auto* tabs = [[NSTabView alloc] initWithFrame:NSZeroRect];
    tabs.translatesAutoresizingMaskIntoConstraints = NO;
    NSArray<NSString*>* tabTitles =
        @[ @"Audio", @"MIDI", @"Plugins", @"MCP", @"Shortcuts", @"Privacy" ];
    for (NSString* title in tabTitles) {
        auto* item = [[NSTabViewItem alloc] initWithIdentifier:title];
        item.label = title;
        item.view = makePanel(title, NSColor.controlBackgroundColor);
        [tabs addTabViewItem:item];
    }

    auto* contentView = [[NSView alloc] initWithFrame:NSZeroRect];
    window.contentView = contentView;
    [contentView addSubview:tabs];
    [NSLayoutConstraint activateConstraints:@[
        [tabs.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor constant:12.0],
        [tabs.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor constant:-12.0],
        [tabs.topAnchor constraintEqualToAnchor:contentView.topAnchor constant:12.0],
        [tabs.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor constant:-12.0],
    ]];

    return window;
}

} // namespace

@interface LaMusicaAppDelegate : NSObject <NSApplicationDelegate, NSMenuItemValidation> {
    std::unique_ptr<lamusica::session::ApplicationSession> _session;
}
@property(strong) NSWindow* mainWindow;
@property(strong) NSWindow* preferencesWindow;
@property(assign) BOOL transportPlaying;
@end

@implementation LaMusicaAppDelegate

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _session = std::make_unique<lamusica::session::ApplicationSession>();
    }
    return self;
}

- (void)newProject:(id)sender {
    (void)sender;
    const auto path = std::filesystem::temp_directory_path() / "LaMusica Untitled.Project.lamusica";
    _session->createProject(path, "Untitled");
    self.mainWindow.title = @"LaMusica - Untitled";
}

- (void)openDocument:(id)sender {
    (void)sender;
    auto* panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK) {
        _session->openProject(std::filesystem::path{panel.URL.path.UTF8String});
        self.mainWindow.title =
            [NSString stringWithFormat:@"LaMusica - %s", _session->status().projectName.c_str()];
    }
}

- (void)saveDocument:(id)sender {
    (void)sender;
    _session->saveProject();
    self.mainWindow.title =
        [NSString stringWithFormat:@"LaMusica - %s", _session->status().projectName.c_str()];
}

- (void)closeDocument:(id)sender {
    (void)sender;
    _session->closeProject();
    [self.mainWindow close];
}

- (void)showPreferences:(id)sender {
    (void)sender;
    if (self.preferencesWindow == nil) {
        self.preferencesWindow = buildPreferencesWindow();
    }
    [self.preferencesWindow makeKeyAndOrderFront:nil];
}

- (void)play:(id)sender {
    (void)sender;
    (void)_session->routeMenuCommand("transport.play");
    self.transportPlaying = YES;
}

- (void)stop:(id)sender {
    (void)sender;
    (void)_session->routeMenuCommand("transport.stop");
    self.transportPlaying = NO;
}

- (void)showPrimaryPanel:(id)sender {
    auto* item = (NSMenuItem*)sender;
    if ([item.title isEqualToString:@"Show Browser"]) {
        (void)_session->routeMenuCommand("view.browser");
    } else if ([item.title isEqualToString:@"Show Timeline"]) {
        (void)_session->routeMenuCommand("view.timeline");
    } else if ([item.title isEqualToString:@"Show Mixer"]) {
        (void)_session->routeMenuCommand("view.mixer");
    } else if ([item.title isEqualToString:@"Show Inspector"]) {
        (void)_session->routeMenuCommand("view.inspector");
    }
    self.mainWindow.title = [NSString stringWithFormat:@"LaMusica - %@", item.title];
}

- (void)rescanPlugins:(id)sender {
    (void)sender;
    self.mainWindow.title = @"LaMusica - Plugin Scan";
}

- (void)showHelp:(id)sender {
    (void)sender;
    self.mainWindow.title = @"LaMusica - Help";
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
    const SEL action = menuItem.action;
    if (action == @selector(saveDocument:) || action == @selector(closeDocument:) ||
        action == @selector(play:) || action == @selector(stop:) ||
        action == @selector(showPrimaryPanel:) || action == @selector(rescanPlugins:)) {
        if (action == @selector(saveDocument:) || action == @selector(closeDocument:)) {
            return _session != nullptr && _session->status().hasOpenProject;
        }
        return self.mainWindow != nil;
    }
    return YES;
}

@end

namespace {

void buildMenuBar(LaMusicaAppDelegate* delegate) {
    auto* mainMenu = [[NSMenu alloc] initWithTitle:@"MainMenu"];
    NSApp.mainMenu = mainMenu;

    auto* appMenuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [mainMenu addItem:appMenuItem];
    auto* appMenu = [[NSMenu alloc] initWithTitle:@"LaMusica"];
    appMenuItem.submenu = appMenu;
    addMenuItem(appMenu, @"Preferences...", @selector(showPreferences:), @",", delegate);
    [appMenu addItem:[NSMenuItem separatorItem]];
    addMenuItem(appMenu, @"Quit LaMusica", @selector(terminate:), @"q", NSApp);

    auto* projectMenuItem = [[NSMenuItem alloc] initWithTitle:@"Project"
                                                       action:nil
                                                keyEquivalent:@""];
    [mainMenu addItem:projectMenuItem];
    auto* projectMenu = [[NSMenu alloc] initWithTitle:@"Project"];
    projectMenuItem.submenu = projectMenu;
    addMenuItem(projectMenu, @"New Project", @selector(newProject:), @"n", delegate);
    addMenuItem(projectMenu, @"Open Project...", @selector(openDocument:), @"o", delegate);
    addMenuItem(projectMenu, @"Save", @selector(saveDocument:), @"s", delegate);
    addMenuItem(projectMenu, @"Close Project", @selector(closeDocument:), @"w", delegate);

    auto* editMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    [mainMenu addItem:editMenuItem];
    auto* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    editMenuItem.submenu = editMenu;
    addMenuItem(editMenu, @"Undo", @selector(undo:), @"z");
    addMenuItem(editMenu, @"Redo", @selector(redo:), @"Z");
    [editMenu addItem:[NSMenuItem separatorItem]];
    addMenuItem(editMenu, @"Cut", @selector(cut:), @"x");
    addMenuItem(editMenu, @"Copy", @selector(copy:), @"c");
    addMenuItem(editMenu, @"Paste", @selector(paste:), @"v");

    auto* transportMenuItem = [[NSMenuItem alloc] initWithTitle:@"Transport"
                                                         action:nil
                                                  keyEquivalent:@""];
    [mainMenu addItem:transportMenuItem];
    auto* transportMenu = [[NSMenu alloc] initWithTitle:@"Transport"];
    transportMenuItem.submenu = transportMenu;
    addMenuItem(transportMenu, @"Play", @selector(play:), @" ", delegate);
    addMenuItem(transportMenu, @"Stop", @selector(stop:), @".", delegate);

    NSArray<NSString*>* panelMenus = @[ @"View", @"Audio", @"MIDI", @"Tools", @"Help" ];
    for (NSString* title in panelMenus) {
        auto* menuItem = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
        [mainMenu addItem:menuItem];
        auto* menu = [[NSMenu alloc] initWithTitle:title];
        menuItem.submenu = menu;
        if ([title isEqualToString:@"Tools"]) {
            addMenuItem(menu, @"Rescan Plugins", @selector(rescanPlugins:), @"", delegate);
        } else if ([title isEqualToString:@"Help"]) {
            addMenuItem(menu, @"LaMusica Help", @selector(showHelp:), @"?", delegate);
        } else if ([title isEqualToString:@"Audio"] || [title isEqualToString:@"MIDI"]) {
            addMenuItem(menu, [NSString stringWithFormat:@"%@ Preferences", title],
                        @selector(showPreferences:), @"", delegate);
        } else {
            addMenuItem(menu, @"Show Browser", @selector(showPrimaryPanel:), @"1", delegate);
            addMenuItem(menu, @"Show Timeline", @selector(showPrimaryPanel:), @"2", delegate);
            addMenuItem(menu, @"Show Mixer", @selector(showPrimaryPanel:), @"3", delegate);
            addMenuItem(menu, @"Show Inspector", @selector(showPrimaryPanel:), @"4", delegate);
        }
    }
}

NSWindow* buildMainWindow() {
    const NSRect frame = NSMakeRect(100.0, 100.0, 1280.0, 800.0);
    auto* window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                            NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                    backing:NSBackingStoreBuffered
                      defer:NO];
    window.title = @"LaMusica";

    auto* root = [[NSStackView alloc] initWithFrame:NSZeroRect];
    root.orientation = NSUserInterfaceLayoutOrientationVertical;
    root.translatesAutoresizingMaskIntoConstraints = NO;
    root.spacing = 1.0;

    auto* transport = makePanel(@"Transport", NSColor.windowBackgroundColor);
    auto* center = [[NSStackView alloc] initWithFrame:NSZeroRect];
    center.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    center.spacing = 1.0;
    auto* browser = makePanel(@"Browser", NSColor.controlBackgroundColor);
    auto* timeline = makePanel(@"Timeline / Piano Roll", NSColor.textBackgroundColor);
    auto* inspector = makePanel(@"Inspector", NSColor.controlBackgroundColor);
    auto* mixer = makePanel(@"Mixer", NSColor.windowBackgroundColor);

    [center addArrangedSubview:browser];
    [center addArrangedSubview:timeline];
    [center addArrangedSubview:inspector];
    [root addArrangedSubview:transport];
    [root addArrangedSubview:center];
    [root addArrangedSubview:mixer];

    [transport.heightAnchor constraintEqualToConstant:64.0].active = YES;
    [browser.widthAnchor constraintEqualToConstant:220.0].active = YES;
    [inspector.widthAnchor constraintEqualToConstant:260.0].active = YES;
    [mixer.heightAnchor constraintEqualToConstant:180.0].active = YES;

    auto* contentView = [[NSView alloc] initWithFrame:NSZeroRect];
    window.contentView = contentView;
    [contentView addSubview:root];
    [NSLayoutConstraint activateConstraints:@[
        [root.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor],
        [root.trailingAnchor constraintEqualToAnchor:contentView.trailingAnchor],
        [root.topAnchor constraintEqualToAnchor:contentView.topAnchor],
        [root.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor],
    ]];

    return window;
}

} // namespace

int main(int argc, char** argv) {
    const lamusica::session::Project project{"Untitled"};
    const lamusica::audio::AudioEngine engine{{}};

    if (hasArgument(argc, argv, "--smoke")) {
        std::cout << "LaMusica app smoke: " << project.name() << " @ " << engine.config().sampleRate
                  << " Hz\n";
        return 0;
    }

    @autoreleasepool {
        [NSApplication sharedApplication];
        NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
        auto* delegate = [[LaMusicaAppDelegate alloc] init];
        NSApp.delegate = delegate;
        buildMenuBar(delegate);
        auto* window = buildMainWindow();
        delegate.mainWindow = window;
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }

    return 0;
}
