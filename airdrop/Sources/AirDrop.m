#import "AirDrop.h"

#import <AppKit/AppKit.h>

@interface AirDrop : NSObject <NSApplicationDelegate, NSSharingServiceDelegate>
@property (nonatomic, copy) NSArray<NSString *> *paths;
@property (nonatomic) BOOL sharingIndividually;
@property (nonatomic) NSUInteger successfulShares;
@property (nonatomic) NSUInteger failedShares;
@property (nonatomic, copy) NSArray<NSURL *> *remainingItems;
@property (nonatomic, strong) NSSharingService *sharingService;
@property (nonatomic, strong) NSWindow *sharingWindow;
@property (nonatomic) BOOL sharingCompleted;
@property (nonatomic, strong) id windowCloseObserver;
- (instancetype)initWithPaths:(NSArray<NSString *> *)paths;
@end

@implementation AirDrop

- (instancetype)initWithPaths:(NSArray<NSString *> *)paths {
    self = [super init];
    if (self != nil) {
        _paths = [paths copy];
    }
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    [self sharePaths:self.paths];

    if (@available(macOS 13.0, *)) {
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    }
}

- (void)sharePaths:(NSArray<NSString *> *)paths {
    NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameSendViaAirDrop];
    if (service == nil) {
        exit(2);
    }
    self.sharingService = service;

    NSMutableArray<NSURL *> *items = [NSMutableArray array];
    NSMutableArray<NSString *> *invalidPaths = [NSMutableArray array];

    for (NSString *path in paths) {
        NSURL *candidate = [NSURL URLWithString:path];
        NSString *scheme = candidate.scheme.lowercaseString;
        if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]) {
            [items addObject:candidate];
            continue;
        }

        NSURL *fileURL = [NSURL fileURLWithPath:path isDirectory:NO];
        if ([NSFileManager.defaultManager fileExistsAtPath:fileURL.path]) {
            [items addObject:fileURL.URLByStandardizingPath];
        } else {
            [invalidPaths addObject:path];
        }
    }

    if (invalidPaths.count > 0) {
        puts("Warning: The following paths are invalid");
        for (NSString *path in invalidPaths) {
            printf("    %s\n", path.UTF8String);
        }
    }

    if (items.count == 0) {
        puts("Warning: No valid files or URLs to share.");
        exit(1);
    }

    printf("Sharing %lu items:\n", (unsigned long)items.count);
    [items enumerateObjectsUsingBlock:^(NSURL *item, NSUInteger index, BOOL *stop) {
        (void)stop;
        printf("  %lu. %s\n", (unsigned long)index + 1, item.absoluteString.UTF8String);
    }];

    BOOL hasURLs = NO;
    BOOL hasFiles = NO;
    for (NSURL *item in items) {
        hasURLs |= [item.scheme.lowercaseString isEqualToString:@"http"] ||
                   [item.scheme.lowercaseString isEqualToString:@"https"];
        hasFiles |= item.isFileURL;
    }

    if ((hasURLs && hasFiles) || ![service canPerformWithItems:items]) {
        [self shareItemsIndividually:items];
    } else {
        self.sharingCompleted = NO;
        service.delegate = self;
        [service performWithItems:items];
    }
}

- (void)shareItemsIndividually:(NSArray<NSURL *> *)items {
    self.sharingIndividually = YES;
    self.successfulShares = 0;
    self.failedShares = 0;
    self.remainingItems = items;
    [self shareNextItem];
}

- (void)shareNextItem {
    if (self.remainingItems.count == 0) {
        printf("Sharing completed: %lu successful, %lu failed\n",
               (unsigned long)self.successfulShares, (unsigned long)self.failedShares);
        exit(self.failedShares > 0 ? 1 : 0);
    }

    NSURL *item = self.remainingItems.firstObject;
    self.remainingItems = [self.remainingItems subarrayWithRange:NSMakeRange(1, self.remainingItems.count - 1)];
    NSSharingService *service = [NSSharingService sharingServiceNamed:NSSharingServiceNameSendViaAirDrop];
    if (service == nil) {
        exit(2);
    }
    self.sharingService = service;
    if ([service canPerformWithItems:@[item]]) {
        self.sharingCompleted = NO;
        service.delegate = self;
        [service performWithItems:@[item]];
    } else {
        fprintf(stderr, "\nError: Cannot share: %s\n", item.absoluteString.UTF8String);
        self.failedShares += 1;
        [self shareNextItem];
    }
}

- (void)sharingService:(NSSharingService *)sharingService didShareItems:(NSArray *)items {
    (void)sharingService;
    self.sharingCompleted = YES;
    [self closeSharingWindow];
    if (self.sharingIndividually) {
        self.successfulShares += 1;
        [self shareNextItem];
    } else {
        printf("Sharing completed: %lu successful\n", (unsigned long)items.count);
        exit(0);
    }
}

- (void)sharingService:(NSSharingService *)sharingService
   didFailToShareItems:(NSArray *)items
                 error:(NSError *)error {
    (void)sharingService;
    (void)items;
    self.sharingCompleted = YES;
    [self closeSharingWindow];
    if (self.sharingIndividually) {
        self.failedShares += 1;
        fprintf(stderr, "\nError: Failed to share item: %s\n", error.localizedDescription.UTF8String);
        [self shareNextItem];
    } else {
        fprintf(stderr, "\nError: %s\n", error.localizedDescription.UTF8String);
        exit(1);
    }
}

- (NSRect)sharingService:(NSSharingService *)sharingService sourceFrameOnScreenForShareItem:(id)item {
    (void)sharingService;
    (void)item;
    return NSMakeRect(0, 0, 400, 100);
}

- (NSWindow *)sharingService:(NSSharingService *)sharingService
    sourceWindowForShareItems:(NSArray *)items
          sharingContentScope:(NSSharingContentScope *)sharingContentScope {
    (void)sharingService;
    (void)items;
    (void)sharingContentScope;

    self.sharingWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1)
                                                     styleMask:NSWindowStyleMaskBorderless
                                                       backing:NSBackingStoreBuffered
                                                         defer:NO];
    [self.sharingWindow center];
    self.sharingWindow.level = NSPopUpMenuWindowLevel;
    self.sharingWindow.alphaValue = 0;
    self.sharingWindow.opaque = NO;
    self.sharingWindow.backgroundColor = NSColor.clearColor;
    [self.sharingWindow orderFront:nil];

    __weak AirDrop *weakSelf = self;
    self.windowCloseObserver = [NSNotificationCenter.defaultCenter
        addObserverForName:NSWindowWillCloseNotification
                    object:nil
                     queue:NSOperationQueue.mainQueue
                usingBlock:^(NSNotification *notification) {
        AirDrop *strongSelf = weakSelf;
        NSWindow *closingWindow = notification.object;
        if (strongSelf == nil || closingWindow == strongSelf.sharingWindow ||
            strongSelf.sharingCompleted) {
            return;
        }

        // No sharing delegate method is called when the picker is dismissed.
        [strongSelf closeSharingWindow];
        exit(0);
    }];

    return self.sharingWindow;
}

- (void)closeSharingWindow {
    if (self.windowCloseObserver != nil) {
        [NSNotificationCenter.defaultCenter removeObserver:self.windowCloseObserver];
        self.windowCloseObserver = nil;
    }
    [self.sharingWindow close];
    self.sharingWindow = nil;
    self.sharingService = nil;
}

@end

int airdrop_run(NSArray<NSString *> *paths) {
    NSApplication *application = [NSApplication sharedApplication];
    AirDrop *delegate = [[AirDrop alloc] initWithPaths:paths];
    application.delegate = delegate;
    [application run];
    return 0;
}
