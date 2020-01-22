/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <utility>

#include "bat/ledger/internal/database/database_activity_info.h"
#include "bat/ledger/internal/database/database_migration.h"
#include "bat/ledger/internal/database/database_publisher_info.h"
#include "bat/ledger/internal/database/database_util.h"
#include "bat/ledger/internal/ledger_impl.h"

using std::placeholders::_1;

namespace braveledger_database {

DatabaseMigration::DatabaseMigration(bat_ledger::LedgerImpl* ledger) :
    ledger_(ledger) {
  activity_info_ = std::make_unique<DatabaseActivityInfo>(ledger_);
  publisher_info_ = std::make_unique<DatabasePublisherInfo>(ledger_);
}

DatabaseMigration::~DatabaseMigration() = default;

void DatabaseMigration::Start(
    const int table_version,
    ledger::ResultCallback callback) {
  const int start_version = table_version + 1;
  auto transaction = ledger::DBTransaction::New();
  int migrated_version = table_version;
  const int target_version = GetCurrentVersion();

  if (target_version == table_version) {
    callback(ledger::Result::LEDGER_OK);
    return;
  }

  for (auto i = start_version; i <= target_version; i++) {
    if (!Migrate(transaction.get(), i)) {
      BLOG(ledger_, ledger::LogLevel::LOG_ERROR) <<
      "DB: Error with MigrateV" << (i - 1) << "toV" << i;
      break;
    }

    BLOG(ledger_, ledger::LogLevel::LOG_INFO) <<
    "DB: Migrated to version " << i;

    migrated_version = i;
  }

  transaction->version = migrated_version;
  transaction->compatible_version = GetCompatibleVersion();
  auto command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::MIGRATE;
  transaction->commands.push_back(std::move(command));

  auto transaction_callback = std::bind(&OnResultCallback,
      _1,
      callback);

  ledger_->RunDBTransaction(std::move(transaction), transaction_callback);
}

bool DatabaseMigration::Migrate(
    ledger::DBTransaction* transaction,
    const int target) {
  if (!activity_info_->Migrate(transaction, target)) {
    return false;
  }

  if (!publisher_info_->Migrate(transaction, target)) {
    return false;
  }

  return true;
}

}  // namespace braveledger_database