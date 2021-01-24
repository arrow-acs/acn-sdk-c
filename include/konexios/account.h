/* Copyright (c) 2017 Arrow Electronics, Inc.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Apache License 2.0
 * which accompanies this distribution, and is available at
 * http://apache.org/licenses/LICENSE-2.0
 * Contributors: Arrow Electronics, Inc.
 */

#ifndef KONEXIOS_ACCOUNT_H_
#define KONEXIOS_ACCOUNT_H_

// create or login this account
int konexios_create_account(const char *name, const char *email, const char *pass);

#define konexios_login_account konexios_create_account

#endif
