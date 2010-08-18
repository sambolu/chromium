// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

// This file contains high-level startup tests for the extensions system. We've
// had many silly bugs where command line flags did not get propagated correctly
// into the services, so we didn't start correctly.

class ExtensionStartupTestBase : public InProcessBrowserTest {
 public:
  ExtensionStartupTestBase() : enable_extensions_(false) {
#if defined(OS_CHROMEOS)
    num_expected_extensions_ = 3;
#else
    num_expected_extensions_ = 4;
#endif
  }

 protected:
  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    EnableDOMAutomation();

    FilePath profile_dir;
    PathService::Get(chrome::DIR_USER_DATA, &profile_dir);
    profile_dir = profile_dir.AppendASCII("Default");
    file_util::CreateDirectory(profile_dir);

    preferences_file_ = profile_dir.AppendASCII("Preferences");
    user_scripts_dir_ = profile_dir.AppendASCII("User Scripts");
    extensions_dir_ = profile_dir.AppendASCII("Extensions");

    if (enable_extensions_) {
      FilePath src_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &src_dir);
      src_dir = src_dir.AppendASCII("extensions").AppendASCII("good");

      file_util::CopyFile(src_dir.AppendASCII("Preferences"),
                          preferences_file_);
      file_util::CopyDirectory(src_dir.AppendASCII("Extensions"),
                               profile_dir, true);  // recursive
    } else {
      command_line->AppendSwitch(switches::kDisableExtensions);
    }

    if (!load_extension_.empty()) {
      command_line->AppendSwitchPath(switches::kLoadExtension, load_extension_);
      command_line->AppendSwitch(switches::kDisableExtensionsFileAccessCheck);
    }
  }

  virtual void TearDown() {
    EXPECT_TRUE(file_util::Delete(preferences_file_, false));

    // TODO(phajdan.jr): Check return values of the functions below, carefully.
    file_util::Delete(user_scripts_dir_, true);
    file_util::Delete(extensions_dir_, true);
  }

  void WaitForServicesToStart(int num_expected_extensions,
                              bool expect_extensions_enabled) {
    ExtensionsService* service = browser()->profile()->GetExtensionsService();
    if (!service->is_ready())
      ui_test_utils::WaitForNotification(NotificationType::EXTENSIONS_READY);
    ASSERT_TRUE(service->is_ready());

    ASSERT_EQ(static_cast<uint32>(num_expected_extensions),
              service->extensions()->size());
    ASSERT_EQ(expect_extensions_enabled, service->extensions_enabled());

    UserScriptMaster* master = browser()->profile()->GetUserScriptMaster();
    if (!master->ScriptsReady()) {
      ui_test_utils::WaitForNotification(
          NotificationType::USER_SCRIPTS_UPDATED);
    }
    ASSERT_TRUE(master->ScriptsReady());
  }

  void TestInjection(bool expect_css, bool expect_script) {
    // Load a page affected by the content script and test to see the effect.
    FilePath test_file;
    PathService::Get(chrome::DIR_TEST_DATA, &test_file);
    test_file = test_file.AppendASCII("extensions")
                         .AppendASCII("test_file.html");

    ui_test_utils::NavigateToURL(browser(), net::FilePathToFileURL(test_file));

    bool result = false;
    ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send("
        L"document.defaultView.getComputedStyle(document.body, null)."
        L"getPropertyValue('background-color') == 'rgb(245, 245, 220)')",
        &result);
    EXPECT_EQ(expect_css, result);

    result = false;
    ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send(document.title == 'Modified')",
        &result);
    EXPECT_EQ(expect_script, result);
  }

  FilePath preferences_file_;
  FilePath extensions_dir_;
  FilePath user_scripts_dir_;
  bool enable_extensions_;
  FilePath load_extension_;

  int num_expected_extensions_;
};


// ExtensionsStartupTest
// Ensures that we can startup the browser with --enable-extensions and some
// extensions installed and see them run and do basic things.

class ExtensionsStartupTest : public ExtensionStartupTestBase {
 public:
  ExtensionsStartupTest() {
    enable_extensions_ = true;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, Test) {
  // 1 component extension and 2 or 3 others, depending on the platform.
  WaitForServicesToStart(num_expected_extensions_, true);
  TestInjection(true, true);
}

// Sometimes times out on Mac.  http://crbug.com/48151
#if defined(OS_MACOSX)
#define MAYBE_NoFileAccess DISABLED_NoFileAccess
#else
#define MAYBE_NoFileAccess NoFileAccess
#endif
// Tests that disallowing file access on an extension prevents it from injecting
// script into a page with a file URL.
IN_PROC_BROWSER_TEST_F(ExtensionsStartupTest, MAYBE_NoFileAccess) {
  // 1 component extension and 2 or 3 others, depending on the platform.
  WaitForServicesToStart(num_expected_extensions_, true);

  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    if (service->AllowFileAccess(service->extensions()->at(i))) {
      service->SetAllowFileAccess(service->extensions()->at(i), false);
      ui_test_utils::WaitForNotification(
           NotificationType::USER_SCRIPTS_UPDATED);
    }
  }

  TestInjection(false, false);
}

// ExtensionsLoadTest
// Ensures that we can startup the browser with --load-extension and see them
// run.

class ExtensionsLoadTest : public ExtensionStartupTestBase {
 public:
  ExtensionsLoadTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &load_extension_);
    load_extension_ = load_extension_
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
  }
};

// Flaky (times out) on Mac/Windows. http://crbug.com/46301.
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_Test FLAKY_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(ExtensionsLoadTest, MAYBE_Test) {
  WaitForServicesToStart(1, false);
  TestInjection(true, true);
}
