# Coding Style Preferences

## Braces

- Use **Allman style** - opening braces on a new line at the same indentation level.
- Exception: one-liner functions can have braces on the same line.
  ```cpp
  // Good - Allman style
  if (condition)
  {
      doSomething();
  }

  // Good - one-liner exception
  const QString& id() const { return m_id; }

  // Bad - K&R style
  if (condition) {
      doSomething();
  }
  ```

## Variable Declarations

- **auto usage**: Only use `auto` for iterators. Use explicit types elsewhere.
  ```cpp
  // Good
  auto it = list.begin();
  QString name = person.name();

  // Bad
  auto name = person.name();
  ```

## Optional Types

- Use `std::optional<>` directly. No type aliases like `Opt<T>`.
- **Do not use `std::optional<QString>`**. Use empty QString instead and check with `isEmpty()`.
  ```cpp
  // Good
  std::optional<int> birthYear;
  QString middleName;  // empty if not set
  if (!middleName.isEmpty()) { ... }

  // Bad
  Opt<QString> middleName;
  std::optional<QString> middleName;
  ```

## Control Structures

- **Always use braces** on `if`, `while`, `for`, `switch`, etc.
  ```cpp
  // Good
  if (condition)
  {
      doSomething();
  }

  // Bad
  if (condition)
      doSomething();
  ```

## Case Statements

- **Never use one-liner case statements**. Always put the body on a new line.
  ```cpp
  // Good
  case Type::Foo:
      return "foo";

  // Bad
  case Type::Foo: return "foo";
  ```

## Line Continuation

- Use **leading operators** on continuation lines, not trailing.
  ```cpp
  // Good
  bool result = condition1
      && condition2
      && condition3;

  // Bad
  bool result = condition1 &&
      condition2 &&
      condition3;
  ```

## Naming Conventions

- Use `json` as the variable name in `toJson()` and `fromJson()` methods, not `obj`.
  ```cpp
  // Good
  QJsonObject toJson() const {
      QJsonObject json;
      json["id"] = m_id;
      return json;
  }

  // Bad
  QJsonObject toJson() const {
      QJsonObject obj;
      obj["id"] = m_id;
      return obj;
  }
  ```

## Qt-Specific

- **Avoid QStringLiteral** unless there's a compelling performance reason.
  ```cpp
  // Good
  json["id"] = m_id;

  // Bad
  json[QStringLiteral("id")] = m_id;
  ```

## Equality Operators

- Use **value-based equality** (compare all fields), not ID-based equality.
  ```cpp
  // Good - compares all fields
  bool operator==(const Person& other) const {
      return m_id == other.m_id
          && m_name == other.m_name
          && m_email == other.m_email;
  }
  ```

## Lambdas

- **Avoid lambdas** in general. Use named helper functions or inline the logic instead.
- **Exception**: Simple one-line lambdas are acceptable for sort comparators and predicates passed to STL algorithms.
  ```cpp
  // Good - simple one-liner for sorting
  std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.name < b.name; });

  // Bad - multi-line or complex lambda
  auto processor = [&](const Item& item) {
      validate(item);
      transform(item);
      return item.result();
  };
  ```

## Include Directives

- **Use filename only** for `#include` statements. The build system sets up include paths for subdirectories.
  ```cpp
  // Good
  #include "FilterService.h"
  #include "FamilyListModel.h"

  // Bad
  #include "../services/FilterService.h"
  #include "../listmodels/FamilyListModel.h"
  ```

## Default Parameters

- **Do not use default parameters.** Require all arguments at every call site.
- Default parameters hide implicit conversions (e.g., pointer silently converting to `bool`) and make call sites ambiguous.
  ```cpp
  // Good - all parameters required
  explicit FamilyTreeModel(DocumentManager* documentManager,
                           Filter* filter,
                           bool checkable,
                           QObject* parent);

  // Bad - defaults allow silent misuse
  explicit FamilyTreeModel(DocumentManager* documentManager,
                           Filter* filter,
                           bool checkable = false,
                           QObject* parent = nullptr);
  ```

## Data Structures

- **Embedded child objects** should be sorted by date where applicable (most recent first).
- Use Qt container types (QList, QString, etc.) throughout since this is a Qt application.
