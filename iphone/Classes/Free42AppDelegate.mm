/*****************************************************************************
 * Free42 -- an HP-42S calculator simulator
 * Copyright (C) 2004-2019  Thomas Okken
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

#import <AudioToolbox/AudioServices.h>
#import <sys/stat.h>
#import <dirent.h>

#import "Free42AppDelegate.h"
#import "RootViewController.h"
#import "StatesView.h"

static Free42AppDelegate *instance;
static char version[32] = "";

@implementation Free42AppDelegate

@synthesize rootViewController;

- (void) applicationDidFinishLaunching:(UIApplication *)application {
    // Override point for customization after application launch
    instance = self;

    [[UIDevice currentDevice] setBatteryMonitoringEnabled:YES];
    [[NSNotificationCenter defaultCenter] addObserver:rootViewController selector:@selector(batteryLevelChanged) name:UIDeviceBatteryLevelDidChangeNotification object:nil];
    [rootViewController batteryLevelChanged];
}

- (void) applicationDidEnterBackground:(UIApplication *)application {
    [rootViewController enterBackground];
}

- (void) applicationWillEnterForeground:(UIApplication *)application {
    [rootViewController batteryLevelChanged];
    [rootViewController leaveBackground];
}

- (void) applicationWillTerminate:(UIApplication *)application {
    [rootViewController quit];
}

+ (const char *) getVersion {
    if (version[0] == 0) {
        NSString *path = [[NSBundle mainBundle] pathForResource:@"VERSION" ofType:nil];
        const char *cpath = [path UTF8String];
        FILE *vfile = fopen(cpath, "r");
        fscanf(vfile, "%s", version);
        fclose(vfile);
    }   
    return version;
}

- (BOOL) application:(UIApplication *)app
            openURL:(NSURL *)url
            options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options; {
    // We ignore the URL and just handle all files with names
    // ending in .f42 or .F42 that happen to be in our Inbox.
    DIR *dir = opendir("Inbox");
    struct dirent *d;
    NSMutableArray *fromNames = [NSMutableArray array];
    while ((d = readdir(dir)) != NULL) {
        size_t len = strlen(d->d_name);
        if (len < 5 || strcasecmp(d->d_name + len - 4, ".f42") != 0)
            continue;
        [fromNames addObject:[NSString stringWithUTF8String:d->d_name]];
    }
    closedir(dir);
    for (int i = 0; i < [fromNames count]; i++) {
        NSString *fromName = [fromNames objectAtIndex:i];
        NSString *fromPath = [NSString stringWithFormat:@"Inbox/%@", fromName];
        fromName = [fromName substringToIndex:[fromName length] - 4];
        NSString *toPath = [NSString stringWithFormat:@"config/%@.f42", fromName];
        struct stat st;
        if (stat([toPath UTF8String], &st) == 0)
            toPath = [NSString stringWithFormat:@"config/%@.f42", [StatesView makeCopyName:fromName]];
        rename([fromPath UTF8String], [toPath UTF8String]);
    }
    return YES;
}

@end
