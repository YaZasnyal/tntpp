//
// Created by blade on 27.01.2024.
//

#ifndef TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H
#define TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H

#include <cassert>
#include <cstdint>

#include <msgpack.hpp>

#include "tntpp_defines.h"

namespace tntpp::detail::iproto
{

/// iproto format details can be found in the tarantool documentation
/// https://www.tarantool.io/en/doc/latest/dev_guide/internals/iproto

using MpUint = std::uint64_t;

using SpaceId = MpUint;
using IndexId = MpUint;

static constexpr SpaceId SPACE_VSPACE = 281;
static constexpr IndexId SPACE_VINDEX = 289;

using OperationId = MpUint;
/// IProto message cannot be more that uint32::max() bytes long according to spec
inline static const unsigned char size_tag = 0xce;
using SizeType = std::uint32_t;

enum class RequestType : MpUint
{
  Ok = 0x00,
  Chunk = 0x80,
  TypeErrorBegin = 0x8000,
  TypeErrorEnd = 0x8999,

  Select = 0x01,
  Insert = 0x02,
  Replace = 0x03,
  Update = 0x04,
  Delete = 0x05,
  Auth = 0x07,
  Eval = 0x08,
  Upsert = 0x09,
  Call = 0x0a,
  Nop = 0x0c,
  Ping = 0x40,
  Id = 0x49,

  Unknown = std::numeric_limits<MpUint>::max()
};

std::string req_type_to_str(RequestType op_type)
{
  switch (op_type) {
    case RequestType::Ok:
      return "Ok";
    case RequestType::TypeErrorBegin:
      return "TypeError";
    case RequestType::TypeErrorEnd:
      return "TypeError";
    case RequestType::Chunk:
      return "Chunk";
    case RequestType::Select:
      return "Select";
    case RequestType::Insert:
      return "Insert";
    case RequestType::Replace:
      return "Replace";
    case RequestType::Update:
      return "Update";
    case RequestType::Delete:
      return "Delete";
    case RequestType::Auth:
      return "Auth";
    case RequestType::Eval:
      return "Eval";
    case RequestType::Upsert:
      return "Upsert";
    case RequestType::Call:
      return "Call";
    case RequestType::Nop:
      return "Nop";
    case RequestType::Ping:
      return "Ping";
    case RequestType::Id:
      return "Id";
    case RequestType::Unknown:
      return "Unknown";
  }
  TNTPP_UNREACHABLE;
};

RequestType int_to_req_type(MpUint type)
{
  switch (static_cast<RequestType>(type)) {
    case RequestType::Ok:
    case RequestType::Chunk:
    case RequestType::TypeErrorBegin:
    case RequestType::TypeErrorEnd:
    case RequestType::Select:
    case RequestType::Insert:
    case RequestType::Replace:
    case RequestType::Update:
    case RequestType::Delete:
    case RequestType::Auth:
    case RequestType::Eval:
    case RequestType::Upsert:
    case RequestType::Call:
    case RequestType::Nop:
    case RequestType::Ping:
    case RequestType::Id:
    case RequestType::Unknown:
      return static_cast<RequestType>(type);
  }
  return RequestType::Unknown;
}

inline bool is_error_req_type(MpUint type)
{
  return type >= static_cast<MpUint>(RequestType::TypeErrorBegin)
      && type <= static_cast<MpUint>(RequestType::TypeErrorEnd);
}

enum class FieldType : MpUint
{
  Version = 0x54,  // Binary protocol version supported by the client (MP_UINT)
  Features = 0x55,  // Supported binary protocol features (MP_ARRAY)
  Sync = 0x01,  // Unique request identifier (MP_UINT)
  SchemaVersion = 0x05,  // Version of the database schema (MP_UINT)
  Timestamp = 0x04,  // Time in seconds since the Unix epoch (MP_DOUBLE)
  SpaceId = 0x10, // Space identifier (MP_UINT)
  IndexId = 0x11, // Index identifier (MP_UINT)
  Limit = 0x12, // Maximum number of tuples in the space (MP_UINT)
  Offset = 0x13, // Number of tuples to skip in the select (MP_UINT)
  Iterator = 0x14, // Iterator type(MP_UINT)
  RequestType = 0x00,  // Request type or response type (MP_UINT)
  Key = 0x20, // Array of index keys in the request (MP_ARRAY)
  Data = 0x30,  // Data passed in the transaction. Can be empty. Used in all requests and responses
                // (MP_OBJECT)
  Error_24,  // IPROTO_ERROR_24 is used in Tarantool versions before 2.4.1 (MP_STR)
  // ...

  StreamId = 0x0a,  // Unique stream identifier (MP_UINT)

  Tuple = 0x21,  // Tuple, arguments, operations, or authentication pair (MP_ARRAY)
  FunctionName = 0x22,  // Name of the called function. Used in IPROTO_CALL (MP_STR)
  Username = 0x23,  // Username. Used in IPROTO_AUTH (MP_STR)
  Expr = 0x27,  // Command argument. Used in IPROTO_EVAL (MP_STR)
  AfterPosition = 0x2e, // The position of a tuple after which space_object:select() starts the search (MP_STR)
  AfterTuple = 0x2f, // A tuple after which space_object:select() starts the search (MP_ARRAY)
  FetchPosition = 0x1f, // If true, space_object:select() returns the position of the last selected tuple
};

inline std::optional<FieldType> int_to_field_type(MpUint type)
{
  switch (static_cast<FieldType>(type)) {
    case FieldType::Version:
    case FieldType::Features:
    case FieldType::Sync:
    case FieldType::SchemaVersion:
    case FieldType::Timestamp:
    case FieldType::SpaceId:
    case FieldType::IndexId:
    case FieldType::Limit:
    case FieldType::Offset:
    case FieldType::Iterator:
    case FieldType::RequestType:
    case FieldType::Key:
    case FieldType::Data:
    case FieldType::Error_24:
    case FieldType::StreamId:
    case FieldType::Tuple:
    case FieldType::FunctionName:
    case FieldType::Username:
    case FieldType::Expr:
    case FieldType::AfterPosition:
    case FieldType::AfterTuple:
    case FieldType::FetchPosition:
      return static_cast<FieldType>(type);
  }
  return std::nullopt;
}

// clang-format off
enum TarantoolError : MpUint
{
  Unknown = 0, // Unknown error
  IllegalParams = 1, // Illegal parameters, %s
  MemoryIssue = 2, // Failed to allocate %u bytes in %s for %s
  TupleFound = 3, // Duplicate key exists in unique index \"%s\" in space \"%s\" with old tuple - %s and new tuple - %s
  TupleNotFound = 4, // Tuple doesn't exist in index '%s' in space '%s'
  Unsupported = 5, // %s does not support %s
  NonMaster = 6, // Can't modify data on a replication slave. My master is: %s
  Readonly = 7, // Can't modify data on a read-only instance
  Injection = 8, // Error injection '%s'
  CreateSpace = 9, // Failed to create space '%s': %s
  SpaceExists = 10, // Space '%s' already exists
  DropSpace = 11, // Can't drop space '%s': %s
  AltSpace = 12, // Can't modify space '%s': %s
  IndexType = 13, // Unsupported index type supplied for index '%s' in space '%s'
  ModifyIndex = 14, // Can't create or modify index '%s' in space '%s': %s
  LastDrop = 15, // Can't drop the primary key in a system space, space '%s'
  TupleFormatLimit = 16, // Tuple format limit reached: %u
  DropPrimaryKey = 17, // Can't drop primary key in space '%s' while secondary keys exist
  KeyPartType = 18, // Supplied key type of part %u does not match index part type: expected %s
  ExactMatch = 19, // Invalid key part count in an exact match (expected %u, got %u)
  InvalidMsgpack = 20, // Invalid MsgPack - %s
  ProcRet = 21, // msgpack.encode: can not encode Lua type '%s'
  TupleNotArray = 22, // Tuple/Key must be MsgPack array
  FieldTypeMismatch = 23, // Tuple field %s type does not match one required by operation: expected %s, got %s
  IndexPartTypeMismatch = 24, // Field %s has type '%s' in one index, but type '%s' in another
  UpdateSplice = 25, // SPLICE error on field %s: %s
  UpdateArgType = 26, // Argument type in operation '%c' on field %s does not match field type: expected %s
  FormatMismatchIndexPart = 27, // Field %s has type '%s' in space format, but type '%s' in index definition
  UnknownUpdateOp = 28, // Unknown UPDATE operation #%d: %s
  UpdateField = 29, // Field %s UPDATE error: %s
  FunctionTxActive = 30, // Transaction is active at return from function
  KeyPartCount = 31, // Invalid key part count (expected [0..%u], got %u)
  ProcLua = 32, // %s
  NoSuchProc = 33, // Procedure '%.*s' is not defined
  NoSuchTrigger = 34, // Trigger '%s' doesn't exist
  NoSuchIndexId = 35, // No index #%u is defined in space '%s'
  NoSuchSpace = 36, // Space '%s' does not exist
  NoSuchFieldNo = 37, // Field %d was not found in the tuple
  ExactFieldCount = 38, // Tuple field count %u does not match space field count %u
  FieldMissing = 39, // Tuple field %s required by space format is missing
  WalIo = 40, // Failed to write to disk
  MoreThanOneTuple = 41, // Get() doesn't support partial keys and non-unique indexes
  AccessDenied = 42, // %s access to %s '%s' is denied for user '%s'
  CreateUser = 43, // Failed to create user '%s': %s
  DropUser = 44, // Failed to drop user or role '%s': %s
  NoSuchUser = 45, // User '%s' is not found
  UserExists = 46, // User '%s' already exists
  CredentialsMismatch = 47, // User not found or supplied credentials are invalid
  UnknownRequestType = 48, // Unknown request type %u
  UnknownSchemaObject = 49, // Unknown object type '%s'
  CreateFunction = 50, // Failed to create function '%s': %s
  NoSuchFunction = 51, // Function '%s' does not exist
  FunctionExists = 52, // Function '%s' already exists
  BeforeReplaceRet = 53, // Invalid return value of space:before_replace trigger: expected tuple or nil
  MultistatementTransaction = 54, // Can not perform %s in a multi-statement transaction
  TriggerExists = 55, // Trigger '%s' already exists
  UserMax = 56, // A limit on the total number of users has been reached: %u
  NoSuchEngine = 57, // Space engine '%s' does not exist
  ReloadCfg = 58, // Can't set option '%s' dynamically
  Cfg = 59, // Incorrect value for option '%s': %s
  SavepointEmptyTx = 60, // Can not set a savepoint in an empty transaction
  NoSuchSavepoint = 61, // Can not roll back to savepoint: the savepoint does not exist
  UnknownReplica = 62, // Replica %s is not registered with replica set %s
  ReplicasetUuidMismatch = 63, // Replica set UUID mismatch: expected %s, got %s
  InvalidUuid = 64, // Invalid UUID: %s
  ReplicasetUuidIsRo = 65, // Can't reset replica set UUID: it is already assigned
  InstanceUuidMismatch = 66, // Instance UUID mismatch: expected %s, got %s
  ReplicaIdIsReserved = 67, // Can't initialize replica id with a reserved value %u
  InvalidOrder = 68, // Invalid LSN order for instance %u: previous LSN = %llu, new lsn = %llu
  MissingRequestField = 69, // Missing mandatory field '%s' in request
  Identifier = 70, // Invalid identifier '%s' (expected printable symbols only or it is too long)
  DropFunction = 71, // Can't drop function %u: %s
  IteratorType = 72, // Unknown iterator type '%s'
  ReplicaMax = 73, // Replica count limit reached: %u
  InvalidXlog = 74, // Failed to read xlog: %lld
  InvalidXlogName = 75, // Invalid xlog name: expected %lld got %lld
  InvalidXlogOrder = 76, // Invalid xlog order: %lld and %lld
  NoConnection = 77, // Connection is not established
  Timeout = 78, // Timeout exceeded
  ActiveTransaction = 79, // Operation is not permitted when there is an active transaction
  CursorNoTransaction = 80, // The transaction the cursor belongs to has ended
  CrossEngineTransaction = 81, // A multi-statement transaction can not use multiple storage engines
  NoSuchRole = 82, // Role '%s' is not found
  RoleExists = 83, // Role '%s' already exists
  CreateRole = 84, // Failed to create role '%s': %s
  IndexExists = 85, // Index '%s' already exists
  SessionClosed = 86, // Session is closed
  RoleLoop = 87, // Granting role '%s' to role '%s' would create a loop
  Grant = 88, // Incorrect grant arguments: %s
  PrivGranted = 89, // User '%s' already has %s access on %s%s
  RoleGranted = 90, // User '%s' already has role '%s'
  PrivNotGranted = 91, // User '%s' does not have %s access on %s '%s'
  RoleNotGranted = 92, // User '%s' does not have role '%s'
  MissingSnapshot = 93, // Can't find snapshot
  CantUpdatePrimaryKey = 94, // Attempt to modify a tuple field which is part of primary index in space '%s'
  UpdateIntegoverflow = 95, // Integer overflow when performing '%c' operation on field %s
  GuestUserPassword = 96, // Setting password for guest user has no effect
  TransactionConflict = 97, // Transaction has been aborted by conflict
  UnsupportedPriv = 98, // Unsupported %s privilege '%s'
  LoadFunction = 99, // Failed to dynamically load function '%s': %s
  FunctionLanguage = 100, // Unsupported language '%s' specified for function '%s'
  RtreeRect = 101, // RTree: %s must be an array with %u (point) or %u (rectangle/box) numeric coordinates
  ProcC = 102, // %s
  UnknownRtreeIndexDistanceType = 103, // Unknown RTREE index distance type %s
  Protocol = 104, // %s
  UpsertUniqueSecondaryKey = 105, // Space %s has a unique secondary index and does not support UPSERT
  WrongIndexRecord = 106, // Wrong record in _index space: got {%s}, expected {%s}
  WrongIndexParts = 107, // Wrong index part %u: %s
  WrongIndexOptions = 108, // Wrong index options: %s
  WrongSchemaVersion = 109, // Wrong schema version, current: %d, in request: %llu
  MemtxMaxTupleSize = 110, // Failed to allocate %u bytes for tuple: tuple is too large. Check 'memtx_max_tuple_size' configuration option.
  WrongSpaceOptions = 111, // Wrong space options: %s
  UnsupportedIndexFeature = 112, // Index '%s' (%s) of space '%s' (%s) does not support %s
  ViewIsRo = 113, // View '%s' is read-only
  NoTransaction = 114, // No active transaction
  System = 115, // %s
  Loading = 116, // Instance bootstrap hasn't finished yet
  ConnectionToSelf = 117, // Connection to self
  KeyPartIsTooLong = 118, // Key part is too long: %u of %u bytes
  Compression = 119, // Compression error: %s
  CheckpointInProgress = 120, // Snapshot is already in progress
  SubStmtMax = 121, // Can not execute a nested statement: nesting limit reached
  CommitInSubStmt = 122, // Can not commit transaction in a nested statement
  RollbackInSubStmt = 123, // Rollback called in a nested statement
  Decompression = 124, // Decompression error: %s
  InvalidXlogType = 125, // Invalid xlog type: expected %s, got %s
  AlreadyRunning = 126, // Failed to lock WAL directory %s and hot_standby mode is off
  IndexFieldCountLimit = 127, // Indexed field count limit reached: %d indexed fields
  LocalInstanceIdIsReadOnly = 128, // The local instance id %u is read-only
  BackupInProgress = 129, // Backup is already in progress
  ReadViewAborted = 130, // The read view is aborted
  InvalidIndexFile = 131, // Invalid INDEX file %s: %s
  InvalidRunFile = 132, // Invalid RUN file: %s
  InvalidVylogFile = 133, // Invalid VYLOG file: %s
  CascadeRollback = 134, // WAL has a rollback in progress
  VyQuotaTimeout = 135, // Timed out waiting for Vinyl memory quota
  PartialKey = 136, // %s index  does not support selects via a partial key (expected %u parts, got %u). Please Consider changing index type to TREE.
  TruncateSystemSpace = 137, // Can't truncate a system space, space '%s'
  LoadModule = 138, // Failed to dynamically load module '%.*s': %s
  VinylMaxTupleSize = 139, // Failed to allocate %u bytes for tuple: tuple is too large. Check 'vinyl_max_tuple_size' configuration option.
  WrongDdVersion = 140, // Wrong _schema version: expected 'major.minor[.patch]'
  WrongSpaceFormat = 141, // Wrong space format field %u: %s
  CreateSequence = 142, // Failed to create sequence '%s': %s
  AltSequence = 143, // Can't modify sequence '%s': %s
  DropSequence = 144, // Can't drop sequence '%s': %s
  NoSuchSequence = 145, // Sequence '%s' does not exist
  SequenceExists = 146, // Sequence '%s' already exists
  SequenceOverflow = 147, // Sequence '%s' has overflowed
  NoSuchIndexName = 148, // No index '%s' is defined in space '%s'
  SpaceFieldIsDuplicate = 149, // Space field '%s' is duplicate
  CantCreateCollation = 150, // Failed to initialize collation: %s.
  WrongCollationOptions = 151, // Wrong collation options: %s
  NullablePrimary = 152, // Primary index of space '%s' can not contain nullable parts
  NoSuchFieldNameInSpace = 153, // Field '%s' was not found in space '%s' format
  TransactionYield = 154, // Transaction has been aborted by a fiber yield
  NoSuchGroup = 155, // Replication group '%s' does not exist
  SqlBindValue = 156, // Bind value for parameter %s is out of range for type %s
  SqlBindType = 157, // Bind value type %s for parameter %s is not supported
  SqlBindParameterMax = 158, // SQL bind parameter limit reached: %d
  SqlExecute = 159, // Failed to execute SQL statement: %s
  UpdateDecimalOverflow = 160, // Decimal overflow when performing operation '%c' on field %s
  SqlBindNotFound = 161, // Parameter %s was not found in the statement
  ActionMismatch = 162, // Field %s contains %s on conflict action, but %s in index parts
  ViewMissingSql = 163, // Space declared as a view must have SQL statement
  ForeignKeyConstraint = 164, // Can not commit transaction: deferred foreign keys violations are not resolved
  NoSuchModule = 165, // Module '%s' does not exist
  NoSuchCollation = 166, // Collation '%s' does not exist
  CreateFkConstraint = 167, // Failed to create foreign key constraint '%s': %s
  DropFkConstraint = 168, // Failed to drop foreign key constraint '%s': %s
  NoSuchConstraint = 169, // Constraint '%s' does not exist in space '%s'
  ConstraintExists = 170, // %s constraint '%s' already exists in space '%s'
  SqlTypeMismatch = 171, // Type mismatch: can not convert %s to %s
  RowidOverflow = 172, // Rowid is overflowed: too many entries in ephemeral space
  DropCollation = 173, // Can't drop collation '%s': %s
  IllegalCollationMix = 174, // Illegal mix of collations
  SqlNoSuchPragma = 175, // Pragma '%s' does not exist
  SqlCantResolveField = 176, // Can’t resolve field '%s'
  IndexExistsInSpace = 177, // Index '%s' already exists in space '%s'
  InconsistentTypes = 178, // Inconsistent types: expected %s got %s
  SqlSyntaxWithPos = 179, // Syntax error at line %d at or near position %d: %s
  SqlStackOverflow = 180, // Failed to parse SQL statement: parser stack limit reached
  SqlSelectWildcard = 181, // Failed to expand '*' in SELECT statement without FROM clause
  SqlStatementEmpty = 182, // Failed to execute an empty SQL statement
  SqlKeywordIsReserved = 183, // At line %d at or near position %d: keyword '%.*s' is reserved. Please use double quotes if '%.*s' is an identifier.
  SqlSyntaxNearToken = 184, // Syntax error at line %d near '%.*s'
  SqlUnknownToken = 185, // At line %d at or near position %d: unrecognized token '%.*s'
  SqlParseGeneric = 186, // %s
  SqlAnalyzeArgument = 187, // ANALYZE statement argument %s is not a base table
  SqlColumnCountMax = 188, // Failed to create space '%s': space column count %d exceeds the limit (%d)
  HexLiteralMax = 189, // Hex literal %s%s length %d exceeds the supported limit (%d)
  IntLiteralMax = 190, // Integer literal %s%s exceeds the supported range [-9223372036854775808, 18446744073709551615]
  SqlParseLimit = 191, // %s %d exceeds the limit (%d)
  IndexDefUnsupported = 192, // %s are prohibited in an index definition
  CkDefUnsupported = 193, // %s are prohibited in a ck constraint definition
  MultikeyIndexMismatch = 194, // Field %s is used as multikey in one index and as single key in another
  CreateCkConstraint = 195, // Failed to create check constraint '%s': %s
  CkConstraintFailed = 196, // Check constraint failed '%s': %s
  SqlColumnCount = 197, // Unequal number of entries in row expression: left side has %u, but right side - %u
  FuncIndexFunc = 198, // Failed to build a key for functional index '%s' of space '%s': %s
  FuncIndexFormat = 199, // Key format doesn't match one defined in functional index '%s' of space '%s': %s
  FuncIndexParts = 200, // Wrong functional index definition: %s
  NoSuchFieldName = 201, // Field '%s' was not found in the tuple
  FuncWrongArgCount = 202, // Wrong number of arguments is passed to %s(): expected %s, got %d
  BootstrapReadonly = 203, // Trying to bootstrap a local read-only instance as master
  SqlFuncWrongRetCount = 204, // SQL expects exactly one argument returned from %s, got %d
  FuncInvalidReturnType = 205, // Function '%s' returned value of invalid type: expected %s got %s
  SqlParseGenericWithPos = 206, // At line %d at or near position %d: %s
  ReplicaNotAnon = 207, // Replica '%s' is not anonymous and cannot register.
  CannotRegister = 208, // Couldn't find an instance to register this replica on.
  SessionSettingInvalidValue = 209, // Session setting %s expected a value of type %s
  SqlPrepare = 210, // Failed to prepare SQL statement: %s
  WrongQueryId = 211, // Prepared statement with id %u does not exist
  SequenceNotStarted = 212, // Sequence '%s' is not started
  NoSuchSessionSetting = 213, // Session setting %s doesn't exist
  UncommittedForeignSyncTxns = 214, // Found uncommitted sync transactions from other instance with id %u
  SyncMasterMismatch = 215, // CONFIRM message arrived for an unknown master id %d, expected %d
  SyncQuorumTimeout = 216, // Quorum collection for a synchronous transaction is timed out
  SyncRollback = 217, // A rollback for a synchronous transaction is received
  TupleMetadataIsTooBig = 218, // Can't create tuple: metadata size %u is too big
  XlogGap = 219, // %s
  TooEarlySubscribe = 220, // Can't subscribe non-anonymous replica %s until join is done
  SqlCantAddAutoinc = 221, // Can't add AUTOINCREMENT: space %s can't feature more than one AUTOINCREMENT field
  QuorumWait = 222, // Couldn't wait for quorum %d: %s
  InterferingPromote = 223, // Instance with replica id %u was promoted first
  ElectionDisabled = 224, // Elections were turned off
  TxnRollback = 225, // Transaction was rolled back
  NotLeader = 226, // The instance is not a leader. New leader is %u
  SyncQueueUnclaimed = 227, // The synchronous transaction queue doesn't belong to any instance
  SyncQueueForeign = 228, // The synchronous transaction queue belongs to other instance with id %u
  UnableToProcessInStream = 229, // Unable to process %s request in stream
  UnableToProcessOutOfStream = 230, // Unable to process %s request out of stream
  TransactionTimeout = 231, // Transaction has been aborted by timeout
  ActiveTimer = 232, // Operation is not permitted if timer is already running
  TupleFieldCountLimit = 233, // Tuple field count limit reached: see box.schema.FIELD_MAX
  CreateConstraint = 234, // Failed to create constraint '%s' in space '%s': %s
  FieldConstraintFailed = 235, // Check constraint '%s' failed for field '%s'
  TupleConstraintFailed = 236, // Check constraint '%s' failed for a tuple
  CreateForeignKey = 237, // Failed to create foreign key '%s' in space '%s': %s
  ForeignKeyIntegrity = 238, // Foreign key '%s' integrity check failed: %s
  FieldForeignKeyFailed = 239, // Foreign key constraint '%s' failed for field '%s': %s
  ComplexForeignKeyFailed = 240, // Foreign key constraint '%s' failed: %s
  WrongSpaceUpgradeOptions = 241, // Wrong space upgrade options: %s
  NoElectionQuorum = 242, // Not enough peers connected to start elections: %d out of minimal required %d
  Ssl = 243, // %s
  SplitBrain = 244, // Split-Brain discovered: %s
  OldTerm = 245, // The term is outdated: old - %llu, new - %llu
  InterferingElections = 246, // Interfering elections started
  IteratorPosition = 247, // Iterator position is invalid
  DefaultValueType = 248, // Type of the default value does not match tuple field %s type: expected %s, got %s
  UnknownAuthMethod = 249, // Unknown authentication method '%s'
  InvalidAuthData = 250, // Invalid '%s' data: %s
  InvalidAuthRequest = 251, // Invalid '%s' request: %s
  WeakPassword = 252, // Password doesn't meet security requirements: %s
  OldPassword = 253, // Password must differ from last %d passwords
  NoSuchSession = 254, // Session %llu does not exist
  WrongSessionType = 255, // Session '%s' is not supported
  PasswordExpired = 256, // Password expired
  AuthDelay = 257, // Too many authentication attempts
  AuthRequired = 258, // Authentication required
  SqlSeqScan = 259, // Scanning is not allowed for %s
  NoSuchEvent = 260, // Unknown event %s
  BootstrapNotUnanimous = 261, // Replica %s chose a different bootstrap leader %s
  CantCheckBootstrapLeader = 262, // Can't check who replica %s chose its bootstrap leader
  BootstrapConnectionNotToAll = 263, // Some replica set members were not specified in box.cfg.replication
  NilUuid = 264, // Nil UUID is reserved and can't be used in replication
  WrongFunctionOptions = 265, // Wrong function options: %s
  MissingSystemSpaces = 266, // Snapshot has no system spaces
  ClusterNameMismatch = 267, // Cluster name mismatch: name '%s' provided in config conflicts with the instance one '%s'
  ReplicasetNameMismatch = 268, // Replicaset name mismatch: name '%s' provided in config conflicts with the instance one '%s'
  InstanceNameDuplicate = 269, // Duplicate replica name %s, already occupied by %s
  InstanceNameMismatch = 270, // Instance name mismatch: name '%s' provided in config conflicts with the instance one '%s'
  SchemaNeedsUpgrade = 271, // Your schema version is %u.%u.%u while Tarantool %s requires a more recent schema version. Please, consider using box.schema.upgrade().
  SchemaUpgradeInProgress = 272, // Schema upgrade is already in progress
  Deprecated = 273, // %s is deprecated
  Unconfigured = 274, // Please call box.cfg{} first
  CreateDefaultFunc = 275, // Failed to create field default function '%s': %s
  DefaultFuncFailed = 276, // Error calling field default function '%s': %s
  InvalidDec = 277, // Invalid decimal: '%s'
  InAnotherPromote = 278, // box.ctl.promote() is already running
  Shutdown = 279, // Server is shutting down
  FieldValueOutOfRange = 280, // The value of field %s exceeds the supported range for type '%s': expected [%s..%s], got %s
};
// clang-format on

TarantoolError int_to_tarantool_error(MpUint ec)
{
  if (ec <= static_cast<MpUint>(TarantoolError::FieldValueOutOfRange)) {
    return static_cast<TarantoolError>(ec);
  }
  return TarantoolError::Unknown;
}

class TarantoolErrorCategory : public boost::system::error_category
{
public:
  [[nodiscard]] const char* name() const noexcept override { return "tarantool_error"; }
  [[nodiscard]] std::string message(int ev) const override
  {
    assert(ev >= 0);
    auto ec = int_to_tarantool_error(static_cast<MpUint>(ev));
    switch (ec) {
      case TarantoolError::Unknown:
        return "Unknown";
      case TarantoolError::IllegalParams:
        return "Illegal parameters, %s";
      case TarantoolError::MemoryIssue:
        return "Failed to allocate %u bytes in %s for %s";
      case TarantoolError::TupleFound:
        return "Duplicate key exists in unique index \"%s\" in space \"%s\" with old tuple - %s "
               "and new tuple - %s";
      case TarantoolError::TupleNotFound:
        return "Tuple doesn't exist in index '%s' in space '%s'";
      case TarantoolError::Unsupported:
        return "%s does not support %s";
      case TarantoolError::NonMaster:
        return "Can't modify data on a replication slave. My master is: %s";
      case TarantoolError::Readonly:
        return "Can't modify data on a read-only instance";
      case TarantoolError::Injection:
        return "Error injection '%s'";
      case TarantoolError::CreateSpace:
        return "Failed to create space '%s': %s";
      case TarantoolError::SpaceExists:
        return "Space '%s' already exists";
      case TarantoolError::DropSpace:
        return "Can't drop space '%s': %s";
      case TarantoolError::AltSpace:
        return "Can't modify space '%s': %s";
      case TarantoolError::IndexType:
        return "Unsupported index type supplied for index '%s' in space '%s'";
      case TarantoolError::ModifyIndex:
        return "Can't create or modify index '%s' in space '%s': %s";
      case TarantoolError::LastDrop:
        return "Can't drop the primary key in a system space, space '%s'";
      case TarantoolError::TupleFormatLimit:
        return "Tuple format limit reached: %u";
      case TarantoolError::DropPrimaryKey:
        return "Can't drop primary key in space '%s' while secondary keys exist";
      case TarantoolError::KeyPartType:
        return "Supplied key type of part %u does not match index part type: expected %s";
      case TarantoolError::ExactMatch:
        return "Invalid key part count in an exact match (expected %u, got %u)";
      case TarantoolError::InvalidMsgpack:
        return "Invalid MsgPack - %s";
      case TarantoolError::ProcRet:
        return "msgpack.encode: can not encode Lua type '%s'";
      case TarantoolError::TupleNotArray:
        return "Tuple/Key must be MsgPack array";
      case TarantoolError::FieldTypeMismatch:
        return "Tuple field %s type does not match one required by operation: expected %s, got %s";
      case TarantoolError::IndexPartTypeMismatch:
        return "Field %s has type '%s' in one index, but type '%s' in another";
      case TarantoolError::UpdateSplice:
        return "SPLICE error on field %s: %s";
      case TarantoolError::UpdateArgType:
        return "Argument type in operation '%c' on field %s does not match field type: expected "
               "%s";
      case TarantoolError::FormatMismatchIndexPart:
        return "Field %s has type '%s' in space format, but type '%s' in index definition";
      case TarantoolError::UnknownUpdateOp:
        return "Unknown UPDATE operation #%d: %s";
      case TarantoolError::UpdateField:
        return "Field %s UPDATE error: %s";
      case TarantoolError::FunctionTxActive:
        return "Transaction is active at return from function";
      case TarantoolError::KeyPartCount:
        return "Invalid key part count (expected [0..%u], got %u)";
      case TarantoolError::ProcLua:
        return "%s";
      case TarantoolError::NoSuchProc:
        return "Procedure '%.*s' is not defined";
      case TarantoolError::NoSuchTrigger:
        return "Trigger '%s' doesn't exist";
      case TarantoolError::NoSuchIndexId:
        return "No index #%u is defined in space '%s'";
      case TarantoolError::NoSuchSpace:
        return "Space '%s' does not exist";
      case TarantoolError::NoSuchFieldNo:
        return "Field %d was not found in the tuple";
      case TarantoolError::ExactFieldCount:
        return "Tuple field count %u does not match space field count %u";
      case TarantoolError::FieldMissing:
        return "Tuple field %s required by space format is missing";
      case TarantoolError::WalIo:
        return "Failed to write to disk";
      case TarantoolError::MoreThanOneTuple:
        return "Get() doesn't support partial keys and non-unique indexes";
      case TarantoolError::AccessDenied:
        return "%s access to %s '%s' is denied for user '%s'";
      case TarantoolError::CreateUser:
        return "Failed to create user '%s': %s";
      case TarantoolError::DropUser:
        return "Failed to drop user or role '%s': %s";
      case TarantoolError::NoSuchUser:
        return "User '%s' is not found";
      case TarantoolError::UserExists:
        return "User '%s' already exists";
      case TarantoolError::CredentialsMismatch:
        return "User not found or supplied credentials are invalid";
      case TarantoolError::UnknownRequestType:
        return "Unknown request type %u";
      case TarantoolError::UnknownSchemaObject:
        return "Unknown object type '%s'";
      case TarantoolError::CreateFunction:
        return "Failed to create function '%s': %s";
      case TarantoolError::NoSuchFunction:
        return "Function '%s' does not exist";
      case TarantoolError::FunctionExists:
        return "Function '%s' already exists";
      case TarantoolError::BeforeReplaceRet:
        return "Invalid return value of space:before_replace trigger: expected tuple or nil";
      case TarantoolError::MultistatementTransaction:
        return "Can not perform %s in a multi-statement transaction";
      case TarantoolError::TriggerExists:
        return "Trigger '%s' already exists";
      case TarantoolError::UserMax:
        return "A limit on the total number of users has been reached: %u";
      case TarantoolError::NoSuchEngine:
        return "Space engine '%s' does not exist";
      case TarantoolError::ReloadCfg:
        return "Can't set option '%s' dynamically";
      case TarantoolError::Cfg:
        return "Incorrect value for option '%s': %s";
      case TarantoolError::SavepointEmptyTx:
        return "Can not set a savepoint in an empty transaction";
      case TarantoolError::NoSuchSavepoint:
        return "Can not roll back to savepoint: the savepoint does not exist";
      case TarantoolError::UnknownReplica:
        return "Replica %s is not registered with replica set %s";
      case TarantoolError::ReplicasetUuidMismatch:
        return "Replica set UUID mismatch: expected %s, got %s";
      case TarantoolError::InvalidUuid:
        return "Invalid UUID: %s";
      case TarantoolError::ReplicasetUuidIsRo:
        return "Can't reset replica set UUID: it is already assigned";
      case TarantoolError::InstanceUuidMismatch:
        return "Instance UUID mismatch: expected %s, got %s";
      case TarantoolError::ReplicaIdIsReserved:
        return "Can't initialize replica id with a reserved value %u";
      case TarantoolError::InvalidOrder:
        return "Invalid LSN order for instance %u: previous LSN = %llu, new lsn = %llu";
      case TarantoolError::MissingRequestField:
        return "Missing mandatory field '%s' in request";
      case TarantoolError::Identifier:
        return "Invalid identifier '%s' (expected printable symbols only or it is too long)";
      case TarantoolError::DropFunction:
        return "Can't drop function %u: %s";
      case TarantoolError::IteratorType:
        return "Unknown iterator type '%s'";
      case TarantoolError::ReplicaMax:
        return "Replica count limit reached: %u";
      case TarantoolError::InvalidXlog:
        return "Failed to read xlog: %lld";
      case TarantoolError::InvalidXlogName:
        return "Invalid xlog name: expected %lld got %lld";
      case TarantoolError::InvalidXlogOrder:
        return "Invalid xlog order: %lld and %lld";
      case TarantoolError::NoConnection:
        return "Connection is not established";
      case TarantoolError::Timeout:
        return "Timeout exceeded";
      case TarantoolError::ActiveTransaction:
        return "Operation is not permitted when there is an active transaction";
      case TarantoolError::CursorNoTransaction:
        return "The transaction the cursor belongs to has ended";
      case TarantoolError::CrossEngineTransaction:
        return "A multi-statement transaction can not use multiple storage engines";
      case TarantoolError::NoSuchRole:
        return "Role '%s' is not found";
      case TarantoolError::RoleExists:
        return "Role '%s' already exists";
      case TarantoolError::CreateRole:
        return "Failed to create role '%s': %s";
      case TarantoolError::IndexExists:
        return "Index '%s' already exists";
      case TarantoolError::SessionClosed:
        return "Session is closed";
      case TarantoolError::RoleLoop:
        return "Granting role '%s' to role '%s' would create a loop";
      case TarantoolError::Grant:
        return "Incorrect grant arguments: %s";
      case TarantoolError::PrivGranted:
        return "User '%s' already has %s access on %s%s";
      case TarantoolError::RoleGranted:
        return "User '%s' already has role '%s'";
      case TarantoolError::PrivNotGranted:
        return "User '%s' does not have %s access on %s '%s'";
      case TarantoolError::RoleNotGranted:
        return "User '%s' does not have role '%s'";
      case TarantoolError::MissingSnapshot:
        return "Can't find snapshot";
      case TarantoolError::CantUpdatePrimaryKey:
        return "Attempt to modify a tuple field which is part of primary index in space '%s'";
      case TarantoolError::UpdateIntegoverflow:
        return "Integer overflow when performing '%c' operation on field %s";
      case TarantoolError::GuestUserPassword:
        return "Setting password for guest user has no effect";
      case TarantoolError::TransactionConflict:
        return "Transaction has been aborted by conflict";
      case TarantoolError::UnsupportedPriv:
        return "Unsupported %s privilege '%s'";
      case TarantoolError::LoadFunction:
        return "Failed to dynamically load function '%s': %s";
      case TarantoolError::FunctionLanguage:
        return "Unsupported language '%s' specified for function '%s'";
      case TarantoolError::RtreeRect:
        return "RTree: %s must be an array with %u (point) or %u (rectangle/box) numeric "
               "coordinates";
      case TarantoolError::ProcC:
        return "%s";
      case TarantoolError::UnknownRtreeIndexDistanceType:
        return "Unknown RTREE index distance type %s";
      case TarantoolError::Protocol:
        return "%s";
      case TarantoolError::UpsertUniqueSecondaryKey:
        return "Space %s has a unique secondary index and does not support UPSERT";
      case TarantoolError::WrongIndexRecord:
        return "Wrong record in _index space: got {%s}, expected {%s}";
      case TarantoolError::WrongIndexParts:
        return "Wrong index part %u: %s";
      case TarantoolError::WrongIndexOptions:
        return "Wrong index options: %s";
      case TarantoolError::WrongSchemaVersion:
        return "Wrong schema version, current: %d, in request: %llu";
      case TarantoolError::MemtxMaxTupleSize:
        return "Failed to allocate %u bytes for tuple: tuple is too large. Check "
               "'memtx_max_tuple_size' configuration option.";
      case TarantoolError::WrongSpaceOptions:
        return "Wrong space options: %s";
      case TarantoolError::UnsupportedIndexFeature:
        return "Index '%s' (%s) of space '%s' (%s) does not support %s";
      case TarantoolError::ViewIsRo:
        return "View '%s' is read-only";
      case TarantoolError::NoTransaction:
        return "No active transaction";
      case TarantoolError::System:
        return "%s";
      case TarantoolError::Loading:
        return "Instance bootstrap hasn't finished yet";
      case TarantoolError::ConnectionToSelf:
        return "Connection to self";
      case TarantoolError::KeyPartIsTooLong:
        return "Key part is too long: %u of %u bytes";
      case TarantoolError::Compression:
        return "Compression error: %s";
      case TarantoolError::CheckpointInProgress:
        return "Snapshot is already in progress";
      case TarantoolError::SubStmtMax:
        return "Can not execute a nested statement: nesting limit reached";
      case TarantoolError::CommitInSubStmt:
        return "Can not commit transaction in a nested statement";
      case TarantoolError::RollbackInSubStmt:
        return "Rollback called in a nested statement";
      case TarantoolError::Decompression:
        return "Decompression error: %s";
      case TarantoolError::InvalidXlogType:
        return "Invalid xlog type: expected %s, got %s";
      case TarantoolError::AlreadyRunning:
        return "Failed to lock WAL directory %s and hot_standby mode is off";
      case TarantoolError::IndexFieldCountLimit:
        return "Indexed field count limit reached: %d indexed fields";
      case TarantoolError::LocalInstanceIdIsReadOnly:
        return "The local instance id %u is read-only";
      case TarantoolError::BackupInProgress:
        return "Backup is already in progress";
      case TarantoolError::ReadViewAborted:
        return "The read view is aborted";
      case TarantoolError::InvalidIndexFile:
        return "Invalid INDEX file %s: %s";
      case TarantoolError::InvalidRunFile:
        return "Invalid RUN file: %s";
      case TarantoolError::InvalidVylogFile:
        return "Invalid VYLOG file: %s";
      case TarantoolError::CascadeRollback:
        return "WAL has a rollback in progress";
      case TarantoolError::VyQuotaTimeout:
        return "Timed out waiting for Vinyl memory quota";
      case TarantoolError::PartialKey:
        return "%s index  does not support selects via a partial key (expected %u parts, got %u). "
               "Please Consider changing index type to TREE.";
      case TarantoolError::TruncateSystemSpace:
        return "Can't truncate a system space, space '%s'";
      case TarantoolError::LoadModule:
        return "Failed to dynamically load module '%.*s': %s";
      case TarantoolError::VinylMaxTupleSize:
        return "Failed to allocate %u bytes for tuple: tuple is too large. Check "
               "'vinyl_max_tuple_size' configuration option.";
      case TarantoolError::WrongDdVersion:
        return "Wrong _schema version: expected 'major.minor[.patch]'";
      case TarantoolError::WrongSpaceFormat:
        return "Wrong space format field %u: %s";
      case TarantoolError::CreateSequence:
        return "Failed to create sequence '%s': %s";
      case TarantoolError::AltSequence:
        return "Can't modify sequence '%s': %s";
      case TarantoolError::DropSequence:
        return "Can't drop sequence '%s': %s";
      case TarantoolError::NoSuchSequence:
        return "Sequence '%s' does not exist";
      case TarantoolError::SequenceExists:
        return "Sequence '%s' already exists";
      case TarantoolError::SequenceOverflow:
        return "Sequence '%s' has overflowed";
      case TarantoolError::NoSuchIndexName:
        return "No index '%s' is defined in space '%s'";
      case TarantoolError::SpaceFieldIsDuplicate:
        return "Space field '%s' is duplicate";
      case TarantoolError::CantCreateCollation:
        return "Failed to initialize collation: %s.";
      case TarantoolError::WrongCollationOptions:
        return "Wrong collation options: %s";
      case TarantoolError::NullablePrimary:
        return "Primary index of space '%s' can not contain nullable parts";
      case TarantoolError::NoSuchFieldNameInSpace:
        return "Field '%s' was not found in space '%s' format";
      case TarantoolError::TransactionYield:
        return "Transaction has been aborted by a fiber yield";
      case TarantoolError::NoSuchGroup:
        return "Replication group '%s' does not exist";
      case TarantoolError::SqlBindValue:
        return "Bind value for parameter %s is out of range for type %s";
      case TarantoolError::SqlBindType:
        return "Bind value type %s for parameter %s is not supported";
      case TarantoolError::SqlBindParameterMax:
        return "SQL bind parameter limit reached: %d";
      case TarantoolError::SqlExecute:
        return "Failed to execute SQL statement: %s";
      case TarantoolError::UpdateDecimalOverflow:
        return "Decimal overflow when performing operation '%c' on field %s";
      case TarantoolError::SqlBindNotFound:
        return "Parameter %s was not found in the statement";
      case TarantoolError::ActionMismatch:
        return "Field %s contains %s on conflict action, but %s in index parts";
      case TarantoolError::ViewMissingSql:
        return "Space declared as a view must have SQL statement";
      case TarantoolError::ForeignKeyConstraint:
        return "Can not commit transaction: deferred foreign keys violations are not resolved";
      case TarantoolError::NoSuchModule:
        return "Module '%s' does not exist";
      case TarantoolError::NoSuchCollation:
        return "Collation '%s' does not exist";
      case TarantoolError::CreateFkConstraint:
        return "Failed to create foreign key constraint '%s': %s";
      case TarantoolError::DropFkConstraint:
        return "Failed to drop foreign key constraint '%s': %s";
      case TarantoolError::NoSuchConstraint:
        return "Constraint '%s' does not exist in space '%s'";
      case TarantoolError::ConstraintExists:
        return "%s constraint '%s' already exists in space '%s'";
      case TarantoolError::SqlTypeMismatch:
        return "Type mismatch: can not convert %s to %s";
      case TarantoolError::RowidOverflow:
        return "Rowid is overflowed: too many entries in ephemeral space";
      case TarantoolError::DropCollation:
        return "Can't drop collation '%s': %s";
      case TarantoolError::IllegalCollationMix:
        return "Illegal mix of collations";
      case TarantoolError::SqlNoSuchPragma:
        return "Pragma '%s' does not exist";
      case TarantoolError::SqlCantResolveField:
        return "Can’t resolve field '%s'";
      case TarantoolError::IndexExistsInSpace:
        return "Index '%s' already exists in space '%s'";
      case TarantoolError::InconsistentTypes:
        return "Inconsistent types: expected %s got %s";
      case TarantoolError::SqlSyntaxWithPos:
        return "Syntax error at line %d at or near position %d: %s";
      case TarantoolError::SqlStackOverflow:
        return "Failed to parse SQL statement: parser stack limit reached";
      case TarantoolError::SqlSelectWildcard:
        return "Failed to expand '*' in SELECT statement without FROM clause";
      case TarantoolError::SqlStatementEmpty:
        return "Failed to execute an empty SQL statement";
      case TarantoolError::SqlKeywordIsReserved:
        return "At line %d at or near position %d: keyword '%.*s' is reserved. Please use double "
               "quotes if '%.*s' is an identifier.";
      case TarantoolError::SqlSyntaxNearToken:
        return "Syntax error at line %d near '%.*s'";
      case TarantoolError::SqlUnknownToken:
        return "At line %d at or near position %d: unrecognized token '%.*s'";
      case TarantoolError::SqlParseGeneric:
        return "%s";
      case TarantoolError::SqlAnalyzeArgument:
        return "ANALYZE statement argument %s is not a base table";
      case TarantoolError::SqlColumnCountMax:
        return "Failed to create space '%s': space column count %d exceeds the limit (%d)";
      case TarantoolError::HexLiteralMax:
        return "Hex literal %s%s length %d exceeds the supported limit (%d)";
      case TarantoolError::IntLiteralMax:
        return "Integer literal %s%s exceeds the supported range [-9223372036854775808, "
               "18446744073709551615]";
      case TarantoolError::SqlParseLimit:
        return "%s %d exceeds the limit (%d)";
      case TarantoolError::IndexDefUnsupported:
        return "%s are prohibited in an index definition";
      case TarantoolError::CkDefUnsupported:
        return "%s are prohibited in a ck constraint definition";
      case TarantoolError::MultikeyIndexMismatch:
        return "Field %s is used as multikey in one index and as single key in another";
      case TarantoolError::CreateCkConstraint:
        return "Failed to create check constraint '%s': %s";
      case TarantoolError::CkConstraintFailed:
        return "Check constraint failed '%s': %s";
      case TarantoolError::SqlColumnCount:
        return "Unequal number of entries in row expression: left side has %u, but right side - "
               "%u";
      case TarantoolError::FuncIndexFunc:
        return "Failed to build a key for functional index '%s' of space '%s': %s";
      case TarantoolError::FuncIndexFormat:
        return "Key format doesn't match one defined in functional index '%s' of space '%s': %s";
      case TarantoolError::FuncIndexParts:
        return "Wrong functional index definition: %s";
      case TarantoolError::NoSuchFieldName:
        return "Field '%s' was not found in the tuple";
      case TarantoolError::FuncWrongArgCount:
        return "Wrong number of arguments is passed to %s(): expected %s, got %d";
      case TarantoolError::BootstrapReadonly:
        return "Trying to bootstrap a local read-only instance as master";
      case TarantoolError::SqlFuncWrongRetCount:
        return "SQL expects exactly one argument returned from %s, got %d";
      case TarantoolError::FuncInvalidReturnType:
        return "Function '%s' returned value of invalid type: expected %s got %s";
      case TarantoolError::SqlParseGenericWithPos:
        return "At line %d at or near position %d: %s";
      case TarantoolError::ReplicaNotAnon:
        return "Replica '%s' is not anonymous and cannot register.";
      case TarantoolError::CannotRegister:
        return "Couldn't find an instance to register this replica on.";
      case TarantoolError::SessionSettingInvalidValue:
        return "Session setting %s expected a value of type %s";
      case TarantoolError::SqlPrepare:
        return "Failed to prepare SQL statement: %s";
      case TarantoolError::WrongQueryId:
        return "Prepared statement with id %u does not exist";
      case TarantoolError::SequenceNotStarted:
        return "Sequence '%s' is not started";
      case TarantoolError::NoSuchSessionSetting:
        return "Session setting %s doesn't exist";
      case TarantoolError::UncommittedForeignSyncTxns:
        return "Found uncommitted sync transactions from other instance with id %u";
      case TarantoolError::SyncMasterMismatch:
        return "CONFIRM message arrived for an unknown master id %d, expected %d";
      case TarantoolError::SyncQuorumTimeout:
        return "Quorum collection for a synchronous transaction is timed out";
      case TarantoolError::SyncRollback:
        return "A rollback for a synchronous transaction is received";
      case TarantoolError::TupleMetadataIsTooBig:
        return "Can't create tuple: metadata size %u is too big";
      case TarantoolError::XlogGap:
        return "%s";
      case TarantoolError::TooEarlySubscribe:
        return "Can't subscribe non-anonymous replica %s until join is done";
      case TarantoolError::SqlCantAddAutoinc:
        return "Can't add AUTOINCREMENT: space %s can't feature more than one AUTOINCREMENT field";
      case TarantoolError::QuorumWait:
        return "Couldn't wait for quorum %d: %s";
      case TarantoolError::InterferingPromote:
        return "Instance with replica id %u was promoted first";
      case TarantoolError::ElectionDisabled:
        return "Elections were turned off";
      case TarantoolError::TxnRollback:
        return "Transaction was rolled back";
      case TarantoolError::NotLeader:
        return "The instance is not a leader. New leader is %u";
      case TarantoolError::SyncQueueUnclaimed:
        return "The synchronous transaction queue doesn't belong to any instance";
      case TarantoolError::SyncQueueForeign:
        return "The synchronous transaction queue belongs to other instance with id %u";
      case TarantoolError::UnableToProcessInStream:
        return "Unable to process %s request in stream";
      case TarantoolError::UnableToProcessOutOfStream:
        return "Unable to process %s request out of stream";
      case TarantoolError::TransactionTimeout:
        return "Transaction has been aborted by timeout";
      case TarantoolError::ActiveTimer:
        return "Operation is not permitted if timer is already running";
      case TarantoolError::TupleFieldCountLimit:
        return "Tuple field count limit reached: see box.schema.FIELD_MAX";
      case TarantoolError::CreateConstraint:
        return "Failed to create constraint '%s' in space '%s': %s";
      case TarantoolError::FieldConstraintFailed:
        return "Check constraint '%s' failed for field '%s'";
      case TarantoolError::TupleConstraintFailed:
        return "Check constraint '%s' failed for a tuple";
      case TarantoolError::CreateForeignKey:
        return "Failed to create foreign key '%s' in space '%s': %s";
      case TarantoolError::ForeignKeyIntegrity:
        return "Foreign key '%s' integrity check failed: %s";
      case TarantoolError::FieldForeignKeyFailed:
        return "Foreign key constraint '%s' failed for field '%s': %s";
      case TarantoolError::ComplexForeignKeyFailed:
        return "Foreign key constraint '%s' failed: %s";
      case TarantoolError::WrongSpaceUpgradeOptions:
        return "Wrong space upgrade options: %s";
      case TarantoolError::NoElectionQuorum:
        return "Not enough peers connected to start elections: %d out of minimal required %d";
      case TarantoolError::Ssl:
        return "%s";
      case TarantoolError::SplitBrain:
        return "Split-Brain discovered: %s";
      case TarantoolError::OldTerm:
        return "The term is outdated: old - %llu, new - %llu";
      case TarantoolError::InterferingElections:
        return "Interfering elections started";
      case TarantoolError::IteratorPosition:
        return "Iterator position is invalid";
      case TarantoolError::DefaultValueType:
        return "Type of the default value does not match tuple field %s type: expected %s, got %s";
      case TarantoolError::UnknownAuthMethod:
        return "Unknown authentication method '%s'";
      case TarantoolError::InvalidAuthData:
        return "Invalid '%s' data: %s";
      case TarantoolError::InvalidAuthRequest:
        return "Invalid '%s' request: %s";
      case TarantoolError::WeakPassword:
        return "Password doesn't meet security requirements: %s";
      case TarantoolError::OldPassword:
        return "Password must differ from last %d passwords";
      case TarantoolError::NoSuchSession:
        return "Session %llu does not exist";
      case TarantoolError::WrongSessionType:
        return "Session '%s' is not supported";
      case TarantoolError::PasswordExpired:
        return "Password expired";
      case TarantoolError::AuthDelay:
        return "Too many authentication attempts";
      case TarantoolError::AuthRequired:
        return "Authentication required";
      case TarantoolError::SqlSeqScan:
        return "Scanning is not allowed for %s";
      case TarantoolError::NoSuchEvent:
        return "Unknown event %s";
      case TarantoolError::BootstrapNotUnanimous:
        return "Replica %s chose a different bootstrap leader %s";
      case TarantoolError::CantCheckBootstrapLeader:
        return "Can't check who replica %s chose its bootstrap leader";
      case TarantoolError::BootstrapConnectionNotToAll:
        return "Some replica set members were not specified in box.cfg.replication";
      case TarantoolError::NilUuid:
        return "Nil UUID is reserved and can't be used in replication";
      case TarantoolError::WrongFunctionOptions:
        return "Wrong function options: %s";
      case TarantoolError::MissingSystemSpaces:
        return "Snapshot has no system spaces";
      case TarantoolError::ClusterNameMismatch:
        return "Cluster name mismatch: name '%s' provided in config conflicts with the instance "
               "one '%s'";
      case TarantoolError::ReplicasetNameMismatch:
        return "Replicaset name mismatch: name '%s' provided in config conflicts with the "
               "instance one '%s'";
      case TarantoolError::InstanceNameDuplicate:
        return "Duplicate replica name %s, already occupied by %s";
      case TarantoolError::InstanceNameMismatch:
        return "Instance name mismatch: name '%s' provided in config conflicts with the instance "
               "one '%s'";
      case TarantoolError::SchemaNeedsUpgrade:
        return "Your schema version is %u.%u.%u while Tarantool %s requires a more recent schema "
               "version. Please, consider using box.schema.upgrade().";
      case TarantoolError::SchemaUpgradeInProgress:
        return "Schema upgrade is already in progress";
      case TarantoolError::Deprecated:
        return "%s is deprecated";
      case TarantoolError::Unconfigured:
        return "Please call box.cfg{} first";
      case TarantoolError::CreateDefaultFunc:
        return "Failed to create field default function '%s': %s";
      case TarantoolError::DefaultFuncFailed:
        return "Error calling field default function '%s': %s";
      case TarantoolError::InvalidDec:
        return "Invalid decimal: '%s'";
      case TarantoolError::InAnotherPromote:
        return "box.ctl.promote() is already running";
      case TarantoolError::Shutdown:
        return "Server is shutting down";
      case TarantoolError::FieldValueOutOfRange:
        return "The value of field %s exceeds the supported range for type '%s': expected "
               "[%s..%s], got %s";
    }
    return "Unknown";
  }
};

static inline const boost::system::error_category& tarantool_error_category()
{
  static const TarantoolErrorCategory instance;
  return instance;
}

class MessageHeader
{
public:
  MpUint request_type {static_cast<MpUint>(RequestType::Unknown)};
  MpUint sync {0};
  MpUint schema_version {0};
  std::optional<MpUint> stream_id {std::nullopt};

  void set_request_type(RequestType req_type) { request_type = static_cast<MpUint>(req_type); }
  [[nodiscard]] bool is_error() const { return is_error_req_type(request_type); }
  [[nodiscard]] boost::system::error_code get_error_code() const
  {
    if (!is_error()) {
      return boost::system::error_code {};
    }
    const MpUint code = static_cast<detail::iproto::MpUint>(request_type)
        - static_cast<detail::iproto::MpUint>(detail::iproto::RequestType::TypeErrorBegin);
    return boost::system::error_code(static_cast<int>(int_to_tarantool_error(code)),
                                     tarantool_error_category());
  }
};

}  // namespace tntpp::detail::iproto

namespace msgpack
{
MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
{
  namespace adaptor
  {

  template<>
  struct convert<tntpp::detail::iproto::MessageHeader>
  {
    msgpack::object const& operator()(msgpack::object const& o,
                                      tntpp::detail::iproto::MessageHeader& v) const
    {
      using FieldType = tntpp::detail::iproto::FieldType;
      if (o.type != msgpack::type::MAP) {
        throw msgpack::type_error();
      }

      for (const auto& obj : o.via.map) {
        if (obj.key.type != msgpack::type::POSITIVE_INTEGER) {
          throw msgpack::type_error();
        }

        auto field_type = tntpp::detail::iproto::int_to_field_type(obj.key.via.u64);
        if (!field_type.has_value()) {
          throw msgpack::type_error();
        }
        switch (*field_type) {
          case FieldType::RequestType: {
            v.request_type = obj.val.convert();
            break;
          }
          case FieldType::Sync: {
            v.sync = obj.val.convert();
            break;
          }
          case FieldType::SchemaVersion: {
            v.schema_version = obj.val.convert();
            break;
          }
          case FieldType::StreamId: {
            tntpp::detail::iproto::MpUint stream_id = obj.val.convert();
            v.stream_id = stream_id;
          }
          default:
            break;
        }
      }
      return o;
    }
  };

  template<>
  struct pack<tntpp::detail::iproto::MessageHeader>
  {
    template<typename Stream>
    msgpack::packer<Stream>& operator()(msgpack::packer<Stream>& s,
                                        tntpp::detail::iproto::MessageHeader const& v) const
    {
      using FieldType = tntpp::detail::iproto::FieldType;
      s.pack_map(v.stream_id ? 3 : 2);
      s.pack(static_cast<tntpp::detail::iproto::MpUint>(FieldType::Sync));
      s.pack(v.sync);
      s.pack(static_cast<tntpp::detail::iproto::MpUint>(FieldType::RequestType));
      s.pack(static_cast<tntpp::detail::iproto::MpUint>(v.request_type));
      if (v.stream_id) {
        s.pack(static_cast<tntpp::detail::iproto::MpUint>(FieldType::StreamId));
        s.pack(v.stream_id.value());
      }
      return s;
    }
  };

  }  // namespace adaptor
}
}  // namespace msgpack

namespace boost::system
{
template<>
struct is_error_condition_enum<tntpp::detail::iproto::TarantoolError> : public std::true_type
{
};
}  // namespace boost::system

#endif  // TARANTOOL_CONNECTOR_IPROTO_TYPEDEFS_H
