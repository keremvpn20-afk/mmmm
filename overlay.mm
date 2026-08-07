#import "overlay.h"
#include "hooks.hpp"
#include "sdk.hpp"
#include "memory.hpp"
#import <UIKit/UIKit.h>
#include <thread>
#include <chrono>

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
    } else {
        self.layer.sublayers = nil;
    }
}

static UIColor* ColorForContainer(int type) {
    switch (type) {
        case 1: return [UIColor colorWithRed:0.94 green:0.62 blue:0.12 alpha:1.0]; // Chest: Turuncu
        case 2: return [UIColor colorWithRed:0.12 green:0.82 blue:0.82 alpha:1.0]; // EnderChest: Camgöbeği
        case 3: return [UIColor colorWithRed:0.55 green:0.55 blue:0.58 alpha:1.0]; // Hopper: Gri
        case 4: return [UIColor colorWithRed:0.12 green:0.94 blue:0.12 alpha:1.0]; // Spawner: Yeşil
        default: return [UIColor whiteColor];
    }
}

static NSString* NameForContainer(int type) {
    switch (type) {
        case 1: return @"Chest";
        case 2: return @"Ender Chest";
        case 3: return @"Hopper";
        case 4: return @"Spawner";
        default: return @"Container";
    }
}

- (void)drawRect:(CGRect)rect {
    CGContextRef context = UIGraphicsGetCurrentContext();
    if (!context) return;

    // ViewMatrix Kameramız
    SDK::Matrix viewMatrix = *(SDK::Matrix*)(Memory::GetBaseAddress() + 0x2A00000);

    CGRect screenBounds = [UIScreen mainScreen].bounds;
    float width = screenBounds.size.width;
    float height = screenBounds.size.height;
    
    // TRACER (Çizgi) Başlangıç Noktası (Ekranın en alt ortası)
    CGPoint tracerStart = CGPointMake(width / 2.0f, height);

    std::vector<Hooks::MappedContainer> localList;
    {
        extern std::mutex containerMutex; 
        std::lock_guard<std::mutex> lock(Hooks::containerMutex);
        localList = Hooks::detectedContainers;
    }

    int listYOffset = 150; 
    
    // RADAR - Bulunan Sandık Sayısı
    if (localList.size() > 0) {
        NSString *title = [NSString stringWithFormat:@"FOUND CONTAINERS (%lu):", localList.size()];
        NSDictionary *attr = @{
            NSFontAttributeName: [UIFont systemFontOfSize:14.0 weight:UIFontWeightBlack],
            NSForegroundColorAttributeName: [UIColor greenColor]
        };
        [title drawAtPoint:CGPointMake(15, listYOffset) withAttributes:attr];
        listYOffset += 25;
    }

    for (const auto& obj : localList) {
        UIColor *color = ColorForContainer(obj.type);
        
        // RADAR - Sandık Koordinatları Listesi
        NSString *listText = [NSString stringWithFormat:@"%@ -> X:%.0f  Y:%.0f  Z:%.0f", NameForContainer(obj.type), obj.worldPos.x, obj.worldPos.y, obj.worldPos.z];
        
        NSDictionary *shadowAttr = @{
            NSFontAttributeName: [UIFont systemFontOfSize:13.0 weight:UIFontWeightBold],
            NSForegroundColorAttributeName: [UIColor blackColor]
        };
        [listText drawAtPoint:CGPointMake(16, listYOffset + 1) withAttributes:shadowAttr];
        
        NSDictionary *textAttr = @{
            NSFontAttributeName: [UIFont systemFontOfSize:13.0 weight:UIFontWeightBold],
            NSForegroundColorAttributeName: color
        };
        [listText drawAtPoint:CGPointMake(15, listYOffset) withAttributes:textAttr];
        
        listYOffset += 20;

        // 3D KUTU ve TRACER ÇİZİMİ
        SDK::Vector2 screen{};
        if (SDK::WorldToScreen(obj.worldPos, screen, viewMatrix, width, height)) {
            
            NSString *labelName = [NSString stringWithFormat:@"%@", NameForContainer(obj.type)];
            
            // Kutu Çizimi
            CGRect boxRect = CGRectMake(screen.x - 20, screen.y - 20, 40, 40);
            
            CGContextSetStrokeColorWithColor(context, [UIColor blackColor].CGColor);
            CGContextSetLineWidth(context, 3.0);
            CGContextStrokeRect(context, boxRect);

            CGContextSetStrokeColorWithColor(context, color.CGColor);
            CGContextSetLineWidth(context, 1.5);
            CGContextStrokeRect(context, boxRect);

            // Kutunun üstüne İsmini Yazma
            NSDictionary *attributes = @{
                NSFontAttributeName: [UIFont systemFontOfSize:11.0 weight:UIFontWeightBold],
                NSForegroundColorAttributeName: color
            };
            [labelName drawAtPoint:CGPointMake(screen.x - 20, screen.y - 35) withAttributes:attributes];

            // TRACER (Lazer Çizgisi) Çizimi
            if (Hooks::drawTracers) {
                // Önce Siyah Gölge Çizgi (Daha belirgin olması için)
                CGContextBeginPath(context);
                CGContextMoveToPoint(context, tracerStart.x, tracerStart.y);
                CGContextAddLineToPoint(context, screen.x, screen.y);
                CGContextSetStrokeColorWithColor(context, [UIColor colorWithRed:0.0 green:0.0 blue:0.0 alpha:0.7].CGColor);
                CGContextSetLineWidth(context, 3.0);
                CGContextStrokePath(context);

                // Üstüne Kendi Renginde Çizgi
                CGContextBeginPath(context);
                CGContextMoveToPoint(context, tracerStart.x, tracerStart.y);
                CGContextAddLineToPoint(context, screen.x, screen.y);
                CGContextSetStrokeColorWithColor(context, color.CGColor);
                CGContextSetLineWidth(context, 1.5);
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
