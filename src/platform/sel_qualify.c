/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * Copyright (c) 2012, LiteStack, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include "src/platform/sel_qualify.h"
#include "src/main/zlog.h"
#include "src/platform/nacl_dep_qualify.h"

void NaClRunSelQualificationTests()
{
  /*
   * update: NaClOsIsSupported() removed since zerovm doesn't
   * need shared memory support it always true
   */

  /* fail if Data Execution Prevention is required but is not supported */
  ZLOGFAIL(!NaClCheckDEP(), EFAULT, "dep not supported");
}
