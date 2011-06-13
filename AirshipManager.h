//
//  AirshipManager.h
//  wesnoth
//
//  Created by jogo on 6/11/11.
//  Copyright 2011 Kronberger Spiele. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "UAStoreFront.h"

@interface AirshipManager : NSObject<UAStoreFrontDelegate> {
    UIViewController* viewController;
    UIView* view;
    NSMutableDictionary* takeOffOptions;
    NSMutableArray* downloadedProductsQueue;
}

@property (nonatomic,assign) UIViewController* viewController;
@property (nonatomic,assign) UIView* view;
@property (nonatomic,retain) NSMutableDictionary* takeOffOptions;
@property (nonatomic,retain) NSMutableArray* downloadedProductsQueue;
+(AirshipManager *)sharedInstance;
+(void)showAirshipOnView:(UIView*)view;
@end
