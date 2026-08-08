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
        self.userInteractionEnabled = NO; // Tıklamalar arkaya geçsin
        
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

// BİREBİR WURST CLIENT RENKLERİ
static UIColor* ColorForContainer(int type) {
    switch (type) {
        case 1: return [UIColor colorWithRed:1.0f green:0.53f blue:0.0f alpha:1.0]; // Normal Chest
        case 2: return [UIColor colorWithRed:0.0f green:1.0f blue:0.53f alpha:1.0]; // Ender Chest
        case 3: return [UIColor colorWithRed:0.6f green:0.6f blue:0.6f alpha:1.0];  // Hopper
        case 4: return [UIColor colorWithRed:1.0f green:0.0f blue:0.0f alpha:1.0];  // Spawner
        case 6: return [UIColor colorWithRed:0.55f green:0.27f blue:0.07f alpha:1.0]; // Barrel
        default: return [UIColor whiteColor];
    }
}

static NSString* NameForContainer(int type) {
    switch (type) {
        case 1: return @"Chest";
        case 2: return @"Ender Chest";
        case 3: return @"Hopper";
        case 4: return @"Spawner";
        case 6: return @"Barrel";
        default: return @"Container";
    }
}

// WURST STYLE KUTU ÇİZİMİ (Gölgeli ve İçi Saydam Renkli)
void DrawWurstBox(CGContextRef context, CGRect rect, UIColor* color) {
    CGContextSetStrokeColorWithColor(context, [UIColor colorWithRed:0.0 green:0.0 blue:0.0 alpha:0.8].CGColor);
    CGContextSetLineWidth(context, 3.0);
    CGContextStrokeRect(context, rect);
    
    CGContextSetStrokeColorWithColor(context, color.CGColor);
    CGContextSetLineWidth(context, 1.0);
    CGContextStrokeRect(context, rect);
    
    CGContextSetFillColorWithColor(context, [color colorWithAlphaComponent:0.15].CGColor);
    CGContextFillRect(context, rect);
}

// WURST STYLE TRACER (Siyah Gölge Üstü Canlı Çizgi)
void DrawWurstTracer(CGContextRef context, CGPoint start, CGPoint end, UIColor* color) {
    CGContextBeginPath(context);
    CGContextMoveToPoint(context, start.x, start.y);
    CGContextAddLineToPoint(context, end.x, end.y);
    CGContextSetStrokeColorWithColor(context, [UIColor colorWithRed:0.0 green:0.0 blue:0.0 alpha:0.8].CGColor);
    CGContextSetLineWidth(context, 2.5);
    CGContextStrokePath(context);

    CGContextBeginPath(context);
    CGContextMoveToPoint(context, start.x, start.y);
    CGContextAddLineToPoint(context, end.x, end.y);
    CGContextSetStrokeColorWithColor(context, color.CGColor);
    CGContextSetLineWidth(context, 1.0); 
    CGContextStrokePath(context);
}

- (void)drawRect:(CGRect)rect {
    if (!Hooks::storageEspEnabled) return;

    CGContextRef context = UIGraphicsGetCurrentContext();
    if (!context) return;

    // TODO: Hâlâ 0x2A00000 kullanıyoruz. Ekranda kayma olursa ilk buradan şüpheleneceğiz.
    SDK::Matrix viewMatrix = *(SDK::Matrix*)(Memory::GetBaseAddress() + 0x2A00000);

    CGRect screenBounds = [UIScreen mainScreen].bounds;
    float width = screenBounds.size.width;
    float height = screenBounds.size.height;
    
    // Wurst Tracer genelde ekranın tam alt orta kısmından (crosshair'in çok altından) çıkar.
    CGPoint tracerStartPos = CGPointMake(width / 2.0f, height); 

    std::vector<Hooks::MappedContainer> localList;
    {
        extern std::mutex containerMutex; 
        std::lock_guard<std::mutex> lock(Hooks::containerMutex);
        localList = Hooks::detectedContainers;
    }

    int listYOffset = 150; 
    
    // WURST RADAR BİLGİSİ (Sol üst taraf)
    if (localList.size() > 0) {
        NSString *title = [NSString stringWithFormat:@"[Wurst] Storage ESP: %lu", localList.size()];
        NSDictionary *attr = @{
            NSFontAttributeName: [UIFont systemFontOfSize:14.0 weight:UIFontWeightBlack],
            NSForegroundColorAttributeName: [UIColor whiteColor]
        };
        [title drawAtPoint:CGPointMake(16, listYOffset + 1) withAttributes:@{NSFontAttributeName: [UIFont systemFontOfSize:14.0 weight:UIFontWeightBlack], NSForegroundColorAttributeName: [UIColor blackColor]}];
        [title drawAtPoint:CGPointMake(15, listYOffset) withAttributes:attr];
        listYOffset += 25;
    }

    for (const auto& obj : localList) {
        UIColor *color = ColorForContainer(obj.type);
        
        // EKRANDA 3D'Yİ 2D'YE ÇEVİR VE ÇİZ (WorldToScreen)
        SDK::Vector2 screen{};
        if (SDK::WorldToScreen(obj.worldPos, screen, viewMatrix, width, height)) {
            
            // Sandık Boyutu Kestirimi (Yaklaşık 30x30)
            CGRect boxRect = CGRectMake(screen.x - 15, screen.y - 15, 30, 30);
            DrawWurstBox(context, boxRect, color);

            // İsim Yazısı
            NSString *labelName = [NSString stringWithFormat:@"%@ [%.1fm]", NameForContainer(obj.type), obj.distance];
            NSDictionary *shadowAttr = @{NSFontAttributeName: [UIFont systemFontOfSize:10.0 weight:UIFontWeightBold], NSForegroundColorAttributeName: [UIColor blackColor]};
            NSDictionary *textAttr = @{NSFontAttributeName: [UIFont systemFontOfSize:10.0 weight:UIFontWeightBold], NSForegroundColorAttributeName: color};
            
            [labelName drawAtPoint:CGPointMake(screen.x - 20, screen.y - 30) withAttributes:shadowAttr]; 
            [labelName drawAtPoint:CGPointMake(screen.x - 21, screen.y - 31) withAttributes:textAttr];   

            if (Hooks::drawTracers) {
                DrawWurstTracer(context, tracerStartPos, CGPointMake(screen.x, screen.y), color);
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
