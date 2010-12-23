#import "SampleOFDelegate.h"
#import "OpenFeint+UserOptions.h"

extern bool gPauseForOpenFeint;
bool gPauseForOpenFeint = false;

@implementation SampleOFDelegate

- (void)dashboardWillAppear
{
	gPauseForOpenFeint = true;
}

- (void)dashboardDidAppear
{
}

- (void)dashboardWillDisappear
{
}

- (void)dashboardDidDisappear
{
	gPauseForOpenFeint = false;
}

- (void)offlineUserLoggedIn:(NSString*)userId
{
	NSLog(@"User logged in, but OFFLINE. UserId: %@", userId);
}

- (void)userLoggedIn:(NSString*)userId
{
	NSLog(@"User logged in. UserId: %@", userId);
}

- (BOOL)showCustomOpenFeintApprovalScreen
{
	return NO;
}

@end