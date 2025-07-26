# Party System Compiler Integration

## Overview

Party 시스템의 컴파일 타임 처리 흐름을 정의합니다.

## Compilation Pipeline

### 1. **Parsing Phase**
```
party HolyPaladin { ... }
    ↓
AST_PARTY_DECL node
    ↓
PartyTypeInfo extraction
```

### 2. **Analysis Phase**
```
Role implementations scan
    ↓
Ability verification
    ↓
FiberMap generation planning
```

### 3. **Code Generation Phase**
```
Static FiberMap C code
    ↓
Context initialization code
    ↓
Dispatch wrapper generation
```

## FiberMap Generation Algorithm

### Step 1: Role Discovery
```c
// For each role slot in party
for (roleSlot in party.roleSlots) {
    // Find matching role implementation
    roleImpl = findRoleForType(roleSlot.slotType)
    
    // Check for parallel block
    if (roleImpl.hasParallelBlock) {
        // Extract scheduler info
        scheduler = parseSchedulerTag(roleImpl.parallelBlock)
        // Add to FiberMap
    }
}
```

### Step 2: Scheduler Tag Resolution
```pergyra
parallel on (gpuFiber) { ... }
    ↓
SCHEDULER_GPU_FIBER
```

Mapping table:
- `mainThread` → `SCHEDULER_MAIN_THREAD`
- `cpuFiber` → `SCHEDULER_CPU_FIBER`
- `gpuFiber` → `SCHEDULER_GPU_FIBER`
- `ioFiber` → `SCHEDULER_IO_FIBER`
- `backgroundThread` → `SCHEDULER_BACKGROUND_THREAD`
- `computeThread` → `SCHEDULER_COMPUTE_THREAD`

### Step 3: Priority Extraction
```pergyra
parallel on (mainThread, priority: High) { ... }
    ↓
PRIORITY_HIGH (75)
```

### Step 4: Execution Mode
```pergyra
// Periodic execution
parallel on (backgroundThread) {
    every (500ms) { ... }
}

// Continuous execution
parallel on (computeThread) {
    continuous { ... }
}
```

## Generated Code Example

### Input: Party Declaration
```pergyra
party DungeonParty {
    role slot tank: Damageable & Taunting
    role slot healer: Healing
    role slot dps: DamageDealing
}

role WarriorTank for Warrior {
    parallel on (mainThread) {
        every (1000ms) {
            UpdateThreat()
        }
    }
}

role PriestHealer for Priest {
    parallel on (backgroundThread) {
        every (500ms) {
            ScanAndHeal()
        }
    }
}
```

### Output: Generated C Code
```c
/* Generated FiberMap for DungeonParty */

// Role parallel functions
static void WarriorTank_parallel(void* instance, void* context) {
    Warrior* self = (Warrior*)instance;
    PartyContext* ctx = (PartyContext*)context;
    
    UpdateThreat(self);
}

static void PriestHealer_parallel(void* instance, void* context) {
    Priest* self = (Priest*)instance;
    PartyContext* ctx = (PartyContext*)context;
    
    ScanAndHeal(self, ctx);
}

// Role metadata
static const RoleParallelMetadata WarriorTank_metadata = {
    .roleName = "WarriorTank",
    .function = (ParallelFunction)WarriorTank_parallel,
    .scheduler = SCHEDULER_MAIN_THREAD,
    .priority = PRIORITY_NORMAL,
    .intervalMs = 1000,
    .continuous = false
};

static const RoleParallelMetadata PriestHealer_metadata = {
    .roleName = "PriestHealer",
    .function = (ParallelFunction)PriestHealer_parallel,
    .scheduler = SCHEDULER_BACKGROUND_THREAD,
    .priority = PRIORITY_NORMAL,
    .intervalMs = 500,
    .continuous = false
};

// Static FiberMap
static const FiberMapEntry DungeonParty_fibermap_entries[] = {
    {
        .roleId = "tank",
        .instanceSlotId = 0,  // Will be filled at runtime
        .parallelFn = (ParallelFunction)WarriorTank_parallel,
        .schedulerTag = SCHEDULER_MAIN_THREAD,
        .priority = PRIORITY_NORMAL,
        .executionIntervalMs = 1000,
        .isContinuous = false
    },
    {
        .roleId = "healer",
        .instanceSlotId = 0,  // Will be filled at runtime
        .parallelFn = (ParallelFunction)PriestHealer_parallel,
        .schedulerTag = SCHEDULER_BACKGROUND_THREAD,
        .priority = PRIORITY_NORMAL,
        .executionIntervalMs = 500,
        .isContinuous = false
    }
};

static const FiberMap DungeonParty_fibermap = {
    .partyTypeName = "DungeonParty",
    .entries = DungeonParty_fibermap_entries,
    .entryCount = 2,
    .cacheKey = 0x1234567890ABCDEF,
    .isStatic = true
};

// Party instance initialization
PartyContext* DungeonParty_init(
    Warrior* tank,
    Priest* healer,
    Mage* dps)
{
    static struct {
        const char* slotName;
        uint32_t slotId;
        void* roleInstance;
        const char** abilities;
        size_t abilityCount;
    } roles[] = {
        {
            .slotName = "tank",
            .slotId = tank->_slotId,
            .roleInstance = tank,
            .abilities = (const char*[]){"Damageable", "Taunting"},
            .abilityCount = 2
        },
        {
            .slotName = "healer",
            .slotId = healer->_slotId,
            .roleInstance = healer,
            .abilities = (const char*[]){"Healing", "Damageable"},
            .abilityCount = 2
        },
        {
            .slotName = "dps",
            .slotId = dps->_slotId,
            .roleInstance = dps,
            .abilities = (const char*[]){"DamageDealing", "Damageable"},
            .abilityCount = 2
        }
    };
    
    static PartyContext context = {
        .roles = roles,
        .roleCount = 3,
        .partyName = "DungeonParty",
        .contextLock = SPINLOCK_INIT
    };
    
    return &context;
}

// Dispatch wrapper
DispatchResult DungeonParty_parallel(
    PartyContext* context,
    JoinStrategy strategy)
{
    // Update slot IDs in fiber map
    FiberMap* map = (FiberMap*)&DungeonParty_fibermap;
    for (size_t i = 0; i < map->entryCount; i++) {
        const char* roleId = map->entries[i].roleId;
        
        // Find role in context
        for (size_t j = 0; j < context->roleCount; j++) {
            if (strcmp(context->roles[j].slotName, roleId) == 0) {
                ((FiberMapEntry*)&map->entries[i])->instanceSlotId = 
                    context->roles[j].slotId;
                break;
            }
        }
    }
    
    return DispatchParallel(map, context, strategy, NULL);
}
```

## Optimization Opportunities

### 1. **Static Dispatch**
When all role bindings are known at compile time:
```c
// Direct function calls instead of function pointers
static inline void DungeonParty_parallel_optimized() {
    // Inline role parallel blocks
    WarriorTank_UpdateThreat(&warrior);
    PriestHealer_ScanAndHeal(&priest);
}
```

### 2. **Scheduler Affinity**
```c
// Pre-allocate fibers to specific CPU cores
static const SchedulerAffinity affinity = {
    .tank = CPU_CORE_0,
    .healer = CPU_CORE_1,
    .dps = CPU_CORE_2
};
```

### 3. **Memory Layout Optimization**
```c
// Pack party data for cache efficiency
struct DungeonPartyPacked {
    // Hot data (frequently accessed)
    struct {
        int32_t tankHealth;
        int32_t healerMana;
        int32_t dpsCombo;
    } hot;
    
    // Cold data (rarely accessed)
    struct {
        char names[3][64];
        // ...
    } cold;
} __attribute__((packed));
```

## Error Detection

### Compile-Time Checks
1. **Missing Abilities**
   ```
   Error: Role 'tank' requires ability 'Taunting' but Warrior does not implement it
   ```

2. **Invalid Scheduler**
   ```
   Error: Unknown scheduler 'quantumThread' in role 'MageQuantum'
   ```

3. **Circular Dependencies**
   ```
   Error: Circular context access detected: 
   Tank -> Healer -> DPS -> Tank
   ```

### Runtime Checks
1. **Null Role Instances**
2. **Scheduler Overload**
3. **Deadlock Detection**

## Integration Points

### 1. **With Type System**
- Verify ability constraints
- Generic party instantiation
- Security level propagation

### 2. **With Effect System**
- Ensure required effects for context access
- Track party-wide effects

### 3. **With Memory Management**
- Slot allocation for party data
- Fiber stack management
- Context cleanup

## Future Extensions

### 1. **Dynamic Parties**
```pergyra
party DynamicRaid {
    role slot members: Array<CombatReady>
    
    func AddMember(member: impl CombatReady) {
        members.Push(member)
        RegenerateFiberMap()
    }
}
```

### 2. **Party Composition**
```pergyra
party MegaRaid {
    include party DungeonParty as group1
    include party DungeonParty as group2
    
    shared strategy: RaidStrategy
}
```

### 3. **Party Templates**
```pergyra
party template Squad<T: Soldier, N: const Int> {
    role slot[N] members: T
    
    static_assert(N >= 3 && N <= 12, "Squad size must be 3-12")
}
```
