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
        self.userInteractionEnabled = NO;
        
        _displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(redraw)];
        [_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
    }
    return self;
}

- (void)redraw {
    if (Hooks::storageEspEnabled) {
        [self setNeedsDisplay];
    }
}

static UIColor* ColorForContainer(int type) {
    switch (type) {
        case 1: return [UIColor colorWithRed:1.0f green:0.53f blue:0.0f alpha:1.0];
        case 2: return [UIColor colorWithRed:0.0f green:1.0f blue:0.53f alpha:1.0];
        case 3: return [UIColor colorWithRed:0.6f green:0.6f blue:0.6f alpha:1.0];
        case 4: return [UIColor colorWithRed:1.0f green:0.0f blue:0.0f alpha:1.0];
        case 5: return [UIColor colorWithRed:0.82f green:0.12f blue:0.82f alpha:1.0];
        case 6: return [UIColor colorWithRed:0.55f green:0.27f blue:0.07f alpha:1.0];
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

static void DrawWurstBox(CGContextRef ctx, CGRect rect, UIColor* color) {
    CGContextSetStrokeColorWithColor(ctx, [UIColor colorWithRed:0 green:0 blue:0 alpha:0.8].CGColor);
    CGContextSetLineWidth(ctx, 3.0);
    CGContextStrokeRect(ctx, rect);
    
    CGContextSetStrokeColorWithColor(ctx, color.CGColor);
    CGContextSetLineWidth(ctx, 1.0);
    CGContextStrokeRect(ctx, rect);
    
    CGContextSetFillColorWithColor(ctx, [color colorWithAlphaComponent:0.15].CGColor);
    CGContextFillRect(ctx, rect);
}

static void DrawWurstTracer(CGContextRef ctx, CGPoint start, CGPoint end, UIColor* color) {
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, start.x, start.y);
    CGContextAddLineToPoint(ctx, end.x, end.y);
    CGContextSetStrokeColorWithColor(ctx, [UIColor colorWithRed:0 green:0 blue:0 alpha:0.8].CGColor);
    CGContextSetLineWidth(ctx, 2.5);
    CGContextStrokePath(ctx);

    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, start.x, start.y);
    CGContextAddLineToPoint(ctx, end.x, end.y);
    CGContextSetStrokeColorWithColor(ctx, color.CGColor);
    CGContextSetLineWidth(ctx, 1.0);
    CGContextStrokePath(ctx);
}

- (void)drawRect:(CGRect)rect {
    if (!Hooks::storageEspEnabled) return;

    CGContextRef context = UIGraphicsGetCurrentContext();
    if (!context) return;

    SDK::Matrix viewMatrix = *(SDK::Matrix*)(Memory::GetBaseAddress() + 0x2A00000);

    CGRect screenBounds = [UIScreen mainScreen].bounds;
    float width = screenBounds.size.width;
    float height = screenBounds.size.height;
    CGPoint tracerStart = CGPointMake(width / 2.0f, height);

    std::vector<Hooks::MappedContainer> localList;
    {
        std::lock_guard<std::mutex> lock(Hooks::containerMutex);
        localList = Hooks::detectedContainers;
    }

    int listY = 150;
    if (localList.size() > 0) {
        NSString *title = [NSString stringWithFormat:@"[Wurst] Storage ESP: %lu", (unsigned long)localList.size()];
        [title drawAtPoint:CGPointMake(16, listY+1) withAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:14.0 weight:UIFontWeightBlack], NSForegroundColorAttributeName: [UIColor blackColor]}];
        [title drawAtPoint:CGPointMake(15, listY) withAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:14.0 weight:UIFontWeightBlack], NSForegroundColorAttributeName: [UIColor whiteColor]}];
        listY += 25;
    }

    for (const auto& obj : localList) {
        UIColor *color = ColorForContainer(obj.type);

        NSString *listText = [NSString stringWithFormat:@"%@ -> X:%.0f Y:%.0f Z:%.0f [%.0fm]", NameForContainer(obj.type), obj.worldPos.x, obj.worldPos.y, obj.worldPos.z, obj.distance];
        [listText drawAtPoint:CGPointMake(16, listY+1) withAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:12.0 weight:UIFontWeightBold], NSForegroundColorAttributeName: [UIColor blackColor]}];
        [listText drawAtPoint:CGPointMake(15, listY) withAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:12.0 weight:UIFontWeightBold], NSForegroundColorAttributeName: color}];
        listY += 18;
        if (listY > 700) break;

        SDK::Vector2 screen{};
        if (SDK::WorldToScreen(obj.worldPos, screen, viewMatrix, width, height)) {
            CGRect boxRect = CGRectMake(screen.x - 15, screen.y - 15, 30, 30);
            DrawWurstBox(context, boxRect, color);

            NSString *label = [NSString stringWithFormat:@"%@ [%.0fm]", NameForContainer(obj.type), obj.distance];
            [label drawAtPoint:CGPointMake(screen.x - 20, screen.y - 30) withAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:10.0 weight:UIFontWeightBold], NSForegroundColorAttributeName: [UIColor blackColor]}];
            [label drawAtPoint:CGPointMake(screen.x - 21, screen.y - 31) withAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:10.0 weight:UIFontWeightBold], NSForegroundColorAttributeName: color}];

            if (Hooks::drawTracers) {
                DrawWurstTracer(context, tracerStart, CGPointMake(screen.x, screen.y), color);
            }
        }
    }
}
@end

namespace Overlay {
    void Initialize() {
        dispatch_async(dispatch_get_main_queue(), ^{
            gEspView = [[MainESPView alloc] initWithFrame:[UIScreen mainScreen].bounds];
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
