// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
#define CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
#pragma once

#include <deque>
#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/glue/resource_type.h"

class Extension;
class GURL;
class URLPattern;

namespace content {
class ResourceThrottle;
}

// This class handles delaying of resource loads that depend on unloaded user
// scripts. For each request that comes in, we check if it depends on a user
// script, and if so, whether that user script is ready; if not, we delay the
// request.
//
// This class lives mostly on the IO thread. It listens on the UI thread for
// updates to loaded extensions.
class UserScriptListener
    : public base::RefCountedThreadSafe<
          UserScriptListener,
          content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver {
 public:
  UserScriptListener();

  // Constructs a ResourceThrottle if the UserScriptListener needs to delay the
  // given URL.  Otherwise, this method returns NULL.
  content::ResourceThrottle* CreateResourceThrottle(
      const GURL& url,
      ResourceType::Type resource_type);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<UserScriptListener>;

  typedef std::list<URLPattern> URLPatterns;

  virtual ~UserScriptListener();

  bool ShouldDelayRequest(const GURL& url, ResourceType::Type resource_type);
  void StartDelayedRequests();

  // Update user_scripts_ready_ based on the status of all profiles. On a
  // transition from false to true, we resume all delayed requests.
  void CheckIfAllUserScriptsReady();

  // Resume any requests that we delayed in order to wait for user scripts.
  void UserScriptsReady(void* profile_id);

  // Clean up per-profile information related to the given profile.
  void ProfileDestroyed(void* profile_id);

  // Appends new url patterns to our list, also setting user_scripts_ready_
  // to false.
  void AppendNewURLPatterns(void* profile_id, const URLPatterns& new_patterns);

  // Replaces our url pattern list. This is only used when patterns have been
  // deleted, so user_scripts_ready_ remains unchanged.
  void ReplaceURLPatterns(void* profile_id, const URLPatterns& patterns);

  // True if all user scripts from all profiles are ready.
  bool user_scripts_ready_;

  // Stores a throttle per URL request that we have delayed.
  class Throttle;
  typedef base::WeakPtr<Throttle> WeakThrottle;
  typedef std::deque<WeakThrottle> WeakThrottleList;
  WeakThrottleList throttles_;

  // Per-profile bookkeeping so we know when all user scripts are ready.
  struct ProfileData;
  typedef std::map<void*, ProfileData> ProfileDataMap;
  ProfileDataMap profile_data_;

  // --- UI thread:

  // Helper to collect the extension's user script URL patterns in a list and
  // return it.
  void CollectURLPatterns(const Extension* extension, URLPatterns* patterns);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserScriptListener);
};

#endif  // CHROME_BROWSER_EXTENSIONS_USER_SCRIPT_LISTENER_H_
