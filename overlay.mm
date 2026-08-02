#import "overlay.h"
#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#import <UIKit/UIKit.h>

@interface MainESPView : UIView
@property (nonatomic, strong) CADisplayLink *displayLink;
@end

static MainESPView *gEspView = nil;

@implementation MainESPView

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = [UIColor clearColor];
        self.userInteractionEnabled = NO; // Click-through layer
        
        // Synced redraw loop at target refresh rate
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(redraw)];
        [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
    }
    return self;
}

- (void)redraw {
    if (Hooks::storageEspEnabled) {
        [self setNeedsDisplay];
    } else {
        // Optimization: clear graphics context on toggle off
        self.layer.sublayers = nil;
    }
}

// Map corresponding container types to theme UI colors
static UIColor* ColorForContainer(int type) {
    switch (type) {
        case 1: return [UIColor colorWithRed:0.94 green:0.62 blue:0.12 alpha:1.0]; // Chest: Gold-Orange
        case 2: return [UIColor colorWithRed:0.12 green:0.82 blue:0.82 alpha:1.0]; // EnderChest: Cyan
        case 3: return [UIColor colorWithRed:0.55 green:0.55 blue:0.58 alpha:1.0]; // Hopper: Grey
        case 4: return [UIColor colorWithRed:0.12 green:0.94 blue:0.12 alpha:1.0]; // Spawner: Green
        case 5: return [UIColor colorWithRed:0.82 green:0.12 blue:0.82 alpha:1.0]; // Shulker: Magenta
        case 6: return [UIColor colorWithRed:0.72 green:0.52 blue:0.32 alpha:1.0]; // Barrel: Brownish wood
        default: return [UIColor whiteColor];
    }
}

static NSString* NameForContainer(int type) {
    switch (type) {
        case 1: return @"Chest";
        case 2: return @"Ender Chest";
        case 3: return @"Hopper";
        case 4: return @"Spawner";
        case 5: return @"Shulker Box";
        case 6: return @"Barrel";
        default: return @"Container";
    }
}

- (void)drawRect:(CGRect)rect {
    CGContextRef context = UIGraphicsGetCurrentContext();
    if (!context) return;

    // ViewMatrix location in minecraftpe memory segment
    SDK::Matrix viewMatrix = *(SDK::Matrix*)(Memory::GetBaseAddress() + 0x2A00000);

    CGRect screenBounds = [UIScreen mainScreen].bounds;
    float width = screenBounds.size.width;
    float height = screenBounds.size.height;

    // Center of screen for tracers origins
    CGPoint screenCenter = CGPointMake(width / 2.0f, height / 2.0f);

    std::vector<Hooks::MappedContainer> localList;
    {
        // Safe access copy to avoid data mutations during drawing
        extern std::mutex containerMutex;
        extern std::vector<Hooks::MappedContainer> detectedContainers;
        std::lock_guard<std::mutex> lock(containerMutex);
        localList = detectedContainers;
    }

    for (const auto& obj : localList) {
        SDK::Vector2 screen{};
        if (SDK::WorldToScreen(obj.worldPos, screen, viewMatrix, width, height)) {
            
            UIColor *color = ColorForContainer(obj.type);
            NSString *labelName = [NSString stringWithFormat:@"%@ [%.1fm]", NameForContainer(obj.type), obj.distance];
            
            // Draw 2D ESP Box around container (estimated standard bounding box size 30x30)
            CGRect boxRect = CGRectMake(screen.x - 15, screen.y - 15, 30, 30);
            
            // Draw outer black shadow for visibility
            CGContextSetStrokeColorWithColor(context, [UIColor blackColor].CGColor);
            CGContextSetLineWidth(context, 2.5);
            CGContextStrokeRect(context, boxRect);

            // Draw primary color frame
            CGContextSetStrokeColorWithColor(context, color.CGColor);
            CGContextSetLineWidth(context, 1.2);
            CGContextStrokeRect(context, boxRect);

            // Draw label indicator text
            NSDictionary *attributes = @{
                NSFontAttributeName: [UIFont systemFontOfSize:9.0 weight:UIFontWeightBold],
                NSForegroundColorAttributeName: color
            };
            [labelName drawAtPoint:CGPointMake(screen.x - 15, screen.y - 28) withAttributes:attributes];

            // Render Tracer snap-lines if configured inside "..." sub-settings
            if (Hooks::drawTracers) {
                CGContextBeginPath(context);
                CGContextMoveToPoint(context, screenCenter.x, screenCenter.y);
                CGContextAddLineToPoint(context, screen.x, screen.y);
                
                // Tracer shadow line
                CGContextSetStrokeColorWithColor(context, [UIColor colorWithRed:0.0 green:0.0 blue:0.0 alpha:0.4].CGColor);
                CGContextSetLineWidth(context, 2.0);
                CGContextStrokePath(context);

                // Main colored Tracer line
                CGContextBeginPath(context);
                CGContextMoveToPoint(context, screenCenter.x, screenCenter.y);
                CGContextAddLineToPoint(context, screen.x, screen.y);
                CGContextSetStrokeColorWithColor(context, color.CGColor);
                CGContextSetLineWidth(context, 0.8);
                CGContextStrokePath(context);
            }
        }
    }
}

@end

namespace Overlay {
    void Initialize() {
        dispatch_async(dispatch_get_main_queue(), ^{
            gEspView = [[MainESPView alloc] initWithFrame:[UIScreen mainScreen].bounds];
            
            // Link transparent viewport overlay to UIWindow hierarchy
            id delegate = [UIApplication sharedApplication].delegate;
            if (delegate && [delegate respondsToSelector:@selector(window)]) {
                UIWindow *win = [delegate performSelector:@selector(window)];
                if (win) {
                    [win addSubview:gEspView];
                    [win bringSubviewToFront:gEspView];
                }
            }
        });
    }
    
    void SetVisible(bool visible) {
        dispatch_async(dispatch_get_main_queue(), ^{
            gEspView.hidden = !visible;
        });
    }
}
