# Pergyra Language Naming Convention

## BSD Style + C# Convention

Pergyra adopts a hybrid naming convention that combines BSD-style clarity with C# conventions for modern readability.

## 1. Basic Rules

### 1.1 Variables
- **Local Variables**: camelCase
  ```pergyra
  let counterSlot = ClaimSlot<Int>()
  let userName = "Alice"
  let isValid = true
  ```

### 1.2 Functions and Methods
- **All Functions**: PascalCase
  ```pergyra
  func ProcessData(input: String) -> Int
  func CalculateSum(a: Int, b: Int) -> Int
  ClaimSlot<T>()
  Write(slot, value)
  ```

### 1.3 Types and Classes
- **Types/Classes/Structs**: PascalCase
  ```pergyra
  class PlayerCharacter { }
  struct Matrix { }
  type TreeNode<T> = { }
  ```

### 1.4 Constants
- **Global Constants**: UPPER_SNAKE_CASE
  ```pergyra
  const MAX_SLOTS = 1024
  const DEFAULT_TTL = 300
  const SECURITY_LEVEL_BASIC = 1
  ```

### 1.5 Properties and Fields
- **Public Properties**: PascalCase
- **Private Fields**: camelCase with underscore prefix
  ```pergyra
  class Player {
      // Public properties
      public Health: Int { get; set; }
      public Name: String { get; set; }
      
      // Private fields
      private _healthSlot: SecureSlot<Int>
      private _nameSlot: Slot<String>
  }
  ```

## 2. Special Cases

### 2.1 Built-in Functions
All built-in functions follow PascalCase:
- `ClaimSlot`, `ClaimSecureSlot`
- `Write`, `Read`, `Release`
- `Parallel`, `Log`, `Panic`

### 2.2 Enum Values
- **Enum Type**: PascalCase
- **Enum Values**: PascalCase
  ```pergyra
  enum SecurityLevel {
      Basic,
      Hardware,
      Encrypted
  }
  ```

### 2.3 Generic Type Parameters
- Single uppercase letter or descriptive PascalCase
  ```pergyra
  func Map<T, U>(items: Array<T>, transform: (T) -> U) -> Array<U>
  class Container<TData> { }
  ```

## 3. File Naming
- **Source Files**: snake_case.pgy
  - `main.pgy`
  - `player_controller.pgy`
  - `secure_slots.pgy`

## 4. Module/Package Naming
- **Packages**: lowercase
  - `pergyra.collections`
  - `pergyra.security`
  - `pergyra.parallel`

## 5. Comparison with Other Languages

| Element | Pergyra | C# | Rust | Go |
|---------|---------|----|----|-----|
| Variables | camelCase | camelCase | snake_case | camelCase |
| Functions | PascalCase | PascalCase | snake_case | PascalCase |
| Types | PascalCase | PascalCase | PascalCase | PascalCase |
| Constants | UPPER_SNAKE | PascalCase | UPPER_SNAKE | PascalCase |
| Private Fields | _camelCase | _camelCase | snake_case | camelCase |

## 6. Examples

### Complete Class Example
```pergyra
class SecureGameState {
    // Constants
    private const MAX_PLAYERS = 100
    private const DEFAULT_HEALTH = 100
    
    // Private fields
    private _playersSlot: SecureSlot<Array<Player>>
    private _stateToken: SecurityToken
    
    // Public properties
    public PlayerCount: Int {
        get { return Read(_playersSlot, _stateToken).Length }
    }
    
    // Constructor
    public func Init() {
        let (slot, token) = ClaimSecureSlot<Array<Player>>(SECURITY_LEVEL_HARDWARE)
        _playersSlot = slot
        _stateToken = token
        Write(_playersSlot, CreateArray<Player>(0), _stateToken)
    }
    
    // Methods
    public func AddPlayer(player: Player) {
        let players = Read(_playersSlot, _stateToken)
        players.Append(player)
        Write(_playersSlot, players, _stateToken)
    }
    
    public func RemovePlayer(playerId: Int) {
        let players = Read(_playersSlot, _stateToken)
        let filtered = players.Filter(p => p.Id != playerId)
        Write(_playersSlot, filtered, _stateToken)
    }
    
    // Destructor
    func Deinit() {
        Release(_playersSlot, _stateToken)
    }
}
```

### Parallel Processing Example
```pergyra
func ParallelMatrixOperation(matrices: Array<Matrix>) -> Array<Matrix> {
    let resultSlot = ClaimSlot<Array<Matrix>>()
    
    Parallel {
        for i in 0..matrices.Length {
            let processed = ProcessMatrix(matrices[i])
            AppendToSlot(resultSlot, processed)
        }
    }
    
    let results = Read(resultSlot)
    Release(resultSlot)
    return results
}
```

## 7. Migration Guide

When converting existing Pergyra code to the new convention:

| Old Style | New Style |
|-----------|-----------|
| `claim_slot` | `ClaimSlot` |
| `write` | `Write` |
| `read` | `Read` |
| `parallel` | `Parallel` |
| `counter_slot` | `counterSlot` |
| `process_data()` | `ProcessData()` |
| `MAX_VALUE` | `MAX_VALUE` (unchanged) |

## 8. Rationale

This naming convention combines:
- **BSD clarity**: Clear distinction between types, functions, and variables
- **C# familiarity**: PascalCase for public APIs makes the language feel modern
- **Consistency**: All public APIs use the same convention
- **Safety**: Private field prefix prevents naming conflicts

The result is a language that feels both professional and approachable, with clear visual distinctions between different code elements.
