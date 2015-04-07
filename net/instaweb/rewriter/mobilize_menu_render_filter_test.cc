/*
 * Copyright 2015 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: morlovich@google.com (Maksim Orlovich)

#include "net/instaweb/rewriter/public/mobilize_menu_render_filter.h"

#include "net/instaweb/rewriter/public/mobilize_menu_filter.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/test_rewrite_driver_factory.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/opt/http/mock_property_page.h"
#include "pagespeed/opt/http/property_cache.h"
#include "pagespeed/opt/http/request_context.h"

namespace net_instaweb {

namespace {

const char kPageUrl[] = "http://test.com/page.html";

// Much simplified version of kActualMenu1 the same in MobilizeMenuFilterTest
const char kContent[] =
    "<nav>"
    "<ul>"
    " <li><a href='/submenu1'>Submenu1</a>"
    "  <ul>"
    "   <li><a href='/a'>A</a></li>"
    "   <li><a href='/b'>B</a><li>"
    "   <li><a href='/c'>C</a></li>"
    "  </ul>"
    " </li>"
    " <li><a href='/submenu2'>Submenu2</a>"
    "  <ul>"
    "   <li><a href='/d'>D</a></li>"
    "   <li><a href='/e'>E</a></li>"
    "   <li><a href='/f'>F</a></li>"
    "  </ul>"
    " </li>"
    "</ul>"
    "</nav>\n";

class MobilizeMenuRenderFilterTest : public RewriteTestBase {
 protected:
  MobilizeMenuRenderFilterTest() : pcache_(NULL), page_(NULL) {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    EnableDebug();  // Make menus readable
    filter_.reset(new MobilizeMenuRenderFilter(rewrite_driver()));
    options()->ClearSignatureForTesting();
    options()->set_mob_always(true);
    server_context()->ComputeSignature(options());
    rewrite_driver()->AppendOwnedPreRenderFilter(filter_.release());

    SetResponseWithDefaultHeaders(kPageUrl, kContentTypeHtml, kContent, 100);

    pcache_ = rewrite_driver()->server_context()->page_property_cache();
    const PropertyCache::Cohort* dom_cohort =
        SetupCohort(pcache_, RewriteDriver::kDomCohort);
    server_context()->set_dom_cohort(dom_cohort);
    ClearDriverAndSetUpPCache();
    Statistics* stats = statistics();
    menus_computed_ =
        stats->GetVariable(MobilizeMenuFilter::kMenusComputed);
    menus_added_ =
        stats->GetVariable(MobilizeMenuRenderFilter::kMenusAdded);
  }

  void ClearDriverAndSetUpPCache() {
    rewrite_driver()->Clear();
    rewrite_driver()->set_request_context(
        RequestContext::NewTestRequestContext(factory()->thread_system()));
    page_ = NewMockPage(kPageUrl);
    rewrite_driver()->set_property_page(page_);
    pcache_->Read(page_);
  }

  virtual bool AddHtmlTags() const { return false; }

  scoped_ptr<MobilizeMenuRenderFilter> filter_;
  PropertyCache* pcache_;
  PropertyPage* page_;
  Variable* menus_computed_;
  Variable* menus_added_;
};

TEST_F(MobilizeMenuRenderFilterTest, BasicOperation) {
  const char kMenu[] =
    "<nav class=\"psmob-nav-panel\"><ul class=\"open\">\n"
    "  <li id=\"psmob-nav-0\"><div>Submenu1</div><ul>\n"
    "    <li id=\"psmob-nav-0-0\"><a href=\"/a\">A</a></li>\n"
    "    <li id=\"psmob-nav-0-1\"><a href=\"/b\">B</a></li>\n"
    "    <li id=\"psmob-nav-0-2\"><a href=\"/c\">C</a></li>\n"
    "    <li id=\"psmob-nav-0-3\"><a href=\"/submenu1\">Submenu1</a>"
        "</li></ul></li>\n"
    "  <li id=\"psmob-nav-1\"><div>Submenu2</div><ul>\n"
    "    <li id=\"psmob-nav-1-0\"><a href=\"/d\">D</a></li>\n"
    "    <li id=\"psmob-nav-1-1\"><a href=\"/e\">E</a></li>\n"
    "    <li id=\"psmob-nav-1-2\"><a href=\"/f\">F</a></li>\n"
    "    <li id=\"psmob-nav-1-3\"><a href=\"/submenu2\">Submenu2</a>"
        "</li></ul></li></ul></nav>";
  // Computes the menu.
  ValidateExpected("page", kContent, StrCat(kContent, kMenu));
  EXPECT_EQ(1, menus_computed_->Get());
  EXPECT_EQ(1, menus_added_->Get());

  // Does the same thing from pcache.
  SetFetchResponse404(kPageUrl);
  ValidateExpected("page", kContent, StrCat(kContent, kMenu));
  EXPECT_EQ(1, menus_computed_->Get());
  EXPECT_EQ(2, menus_added_->Get());
}

TEST_F(MobilizeMenuRenderFilterTest, HandleFailure) {
  // Note that Done(false) makes computation fail, 404 doesn't.
  SetFetchFailOnUnexpected(false);
  ValidateExpected("not_page", kContent,
                   StrCat(kContent, "<!--No computed menu-->"));
  EXPECT_EQ(0, menus_computed_->Get());
  EXPECT_EQ(0, menus_added_->Get());
}

}  // namespace

}  // namespace net_instaweb
