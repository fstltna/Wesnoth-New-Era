//
//  AirshipManager.m
//  wesnoth
//
//  Created by jogo on 6/11/11.
//  Copyright 2011 Kronberger Spiele. All rights reserved.
//

#import "AirshipManager.h"
#import "UAirship.h"
#import "UAStoreFront.h"
#import "UAStoreFrontUI.h"

@implementation AirshipManager
@synthesize viewController,view,takeOffOptions,downloadedProductsQueue;
+(AirshipManager *)sharedInstance {
    static AirshipManager *myInstance = nil;
    if (nil == myInstance) {
        myInstance  = [[[self class] alloc] init];
	}
    return myInstance;
}

-(id) init{
	if( (self=[super init])) {
        [UAStoreFront useCustomUI:[UAStoreFrontUI class]];//UAStoreFrontUI is the sample implementation     
		self.takeOffOptions = [[[NSMutableDictionary alloc] init] autorelease];
        //[takeOffOptions setValue:launchOptions forKey:UAirshipTakeOffOptionsLaunchOptionsKey];

        self.downloadedProductsQueue = [[[NSMutableArray alloc]init]autorelease];
        [[UAStoreFront shared] setDelegate:self];
        
	}
	return self;
}

-(void)dealloc{
    self.downloadedProductsQueue = nil;
    self.takeOffOptions = nil;
    [super dealloc];
}

+(void)showAirshipOnView:(UIView*)view{
    [self sharedInstance].view = view;
    
    // Create Airship singleton that's used to talk to Urban Airship servers.
    // Please populate AirshipConfig.plist with your info from http://go.urbanairship.com
    [UAirship takeOff:[self sharedInstance].takeOffOptions];
    
    // Recommended way to present StoreFront. Alternatively you can open to a specific product detail.
    //[UAStoreFront displayStoreFront:self withProductID:@"oxygen34"];
    //there is no ViewController in Wesnoth or it is hidden too well in SDL
    //luckily UAStoreFront only uses methods UIView will respond too
    [UAStoreFront displayStoreFront:(UIViewController*)[self sharedInstance].view animated:YES];
    
    // Specify the sorting of the list of products.
    [UAStoreFront setOrderBy:UAContentsDisplayOrderPrice ascending:YES];

    
    //for testing uncomment this method
    //[[self sharedInstance] productsDownloadProgress:100.0f count:0];
}


//unsure in which order they will be called, I will copy after downloads have finished, we have to test that
-(void)productPurchased:(UAProduct*) product {
    //UALOG(@"[StoreFrontDelegate] Purchased: %@ -- %@", product.productIdentifier, product.title);
    //perhaps will should store that somewhere in NSUserDefaults
}

- (void)productsDownloadProgress:(float)progress count:(int)count {
    //UALOG(@"[StoreFrontDelegate] productsDownloadProgress: %f count: %d", progress, count);
    if (count == 0) {
        //UALOG(@"Downloads complete");

        NSFileManager* fileManager = [[NSFileManager alloc] init];
        
        //we need to move files to NSDocumentsDirectory/wesnoth1.0/data/campaigns
        NSString* documentsPath  = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
        NSString* destinationDirectory =  [documentsPath stringByAppendingString:@"/wesnoth1.0/data/campaigns/"];
        
        //we need to copy from NSLibraryDirectory/ua/downloads/<product_id>/<contents of zip file>
        //NSLibraryDirectory
        NSString* libraryPath = [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) objectAtIndex:0];
        //NSLibraryDirectory/ua/downloads/
        NSString* uaDownloadsPath = [libraryPath stringByAppendingString:@"/ua/downloads/"];
        NSArray* downloadedFolders = [fileManager contentsOfDirectoryAtPath:uaDownloadsPath  error:nil];
        //NSLibraryDirectory/ua/downloads/<product_id>/
        for(NSString* downloadedFolder in downloadedFolders){
            NSString* fullPathWithID = [uaDownloadsPath stringByAppendingString:downloadedFolder];
            NSString* campaignFolder = [[fileManager contentsOfDirectoryAtPath:fullPathWithID error:nil] objectAtIndex:0];
            //skip singlefiles in download directory, e.g. .DS_store if one edits by hand 
            if(!campaignFolder)
                continue;
            NSString* fullPathCampaignFolder = [fullPathWithID stringByAppendingFormat:@"/%@", campaignFolder];
            NSString* fullDestinationPath = [destinationDirectory stringByAppendingString:campaignFolder];
            //NSLog(@"%@",fullPathCampaignFolder);
            //NSLog(@"%@",fullDestinationPath);
            //move from library to documents
            [fileManager moveItemAtPath:fullPathCampaignFolder toPath:fullDestinationPath error:nil];
            //remove the now empty ua/<id> folder
            [fileManager removeItemAtPath:fullPathWithID error:nil];
        }
        //we need to find a way to reload config afterwards, at the moment the user has to restart the app to get access to the new campaigns
    }
}


@end
