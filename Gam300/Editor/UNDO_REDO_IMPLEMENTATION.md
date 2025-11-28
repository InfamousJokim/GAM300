# Undo/Redo System Implementation

## Overview
Implemented a complete Command Pattern-based undo/redo system for the game engine editor. The system supports transform changes (both 3D and 2D GUI sprites), entity creation/deletion, parent-child hierarchy operations, component property changes, and sprite component modifications.

## Files Modified

### New Files
- `Editor/src/Commands/UndoRedo.h` - Command pattern implementation with all command types

### Modified Files
1. **Editor/src/Editor.h** - Added CommandHistory member and getter
2. **Editor/src/Editor.cpp** - Initialize CommandHistory and add Ctrl+Z/Ctrl+Y shortcuts
3. **Editor/src/Panels/ViewportPanel.h** - Added gizmo state tracking for undo (both 2D and 3D)
4. **Editor/src/Panels/ViewportPanel.cpp** - Record transform commands from gizmo operations (2D and 3D)
5. **Editor/src/Panels/HierarchyPanel.cpp** - Record delete, reparent, duplicate, and unparent commands
6. **Editor/src/Panels/MenuBarPanel.cpp** - Record entity creation commands
7. **Editor/src/Panels/Inspector/InspectorPanel.h** - Added tracking state for sprite and transform edits
8. **Editor/src/Panels/Inspector/InspectorPanel.cpp** - Record property change commands for sprite and transform components
9. **Engine/BoomEngine/src/Application/Application.cpp** - Fixed GUI sprite rendering to use world transforms (hierarchy support)

## Command Types

### 1. TransformCommand
Records transform changes (position, rotation, scale) from ImGuizmo manipulation.
- Stores old and new transform states
- Uses UIDs for entity stability
- Only records if transform actually changed

### 2. CreateEntityCommand
Records entity creation from menu or shortcuts.
- Pre-generates UID in constructor
- Supports redo by recreating with same UID
- Maintains parent relationships

### 3. DeleteEntityCommand
Records entity deletion with full state preservation.
- Captures complete entity hierarchy (children included)
- Stores transform, model, camera, **sprite**, and other components
- Perfectly restores entity on undo including all component data

### 4. ReparentCommand
Records parent-child relationship changes.
- Preserves world transform during reparenting
- Stores old and new parent UIDs
- Handles unparenting (setting parent to null)
- Works with both 3D entities and 2D GUI sprites

### 5. ComponentPropertyCommand (Template)
Records component property changes from Inspector panel.
- Generic template that works with any component type
- Captures before/after state of entire component
- Uses UIDs for entity stability
- Supports SpriteComponent, TransformComponent, and other components

### 6. DuplicateEntityCommand
Records entity duplication operations.
- Duplicates entire hierarchy (including children)
- Preserves all components (including SpriteComponent)
- Undo deletes the duplicated entity tree
- Redo recreates the duplicate

## Keyboard Shortcuts

- **Ctrl+Z** - Undo last operation
- **Ctrl+Y** or **Ctrl+Shift+Z** - Redo last undone operation

## Supported Operations

### Viewport Operations (3D Entities)
| Operation | Panel | Trigger | Undoable |
|-----------|-------|---------|----------|
| Move Entity (3D) | Viewport | Gizmo (W key) | ✅ |
| Rotate Entity (3D) | Viewport | Gizmo (E key) | ✅ |
| Scale Entity (3D) | Viewport | Gizmo (R key) | ✅ |

### Viewport Operations (2D GUI Sprites)
| Operation | Panel | Trigger | Undoable |
|-----------|-------|---------|----------|
| Move Sprite (2D) | Viewport | 2D Gizmo (W key) | ✅ |
| Rotate Sprite (2D) | Viewport | 2D Gizmo (E key) | ✅ |
| Scale Sprite (2D) | Viewport | 2D Gizmo (R key) | ✅ |

### Inspector Operations (Transform)
| Operation | Panel | Trigger | Undoable |
|-----------|-------|---------|----------|
| Change Position | Inspector | Drag translate values | ✅ |
| Change Rotation | Inspector | Drag rotation values | ✅ |
| Change Scale | Inspector | Drag scale values | ✅ |

### Inspector Operations (Sprite Component)
| Operation | Panel | Trigger | Undoable |
|-----------|-------|---------|----------|
| Toggle GUI Overlay | Inspector | Click GUI checkbox | ✅ |
| Change Sprite Texture | Inspector | Drag texture asset | ✅ |
| Change Sprite Color | Inspector | Edit color picker | ✅ |

### Hierarchy Operations
| Operation | Panel | Trigger | Undoable |
|-----------|-------|---------|----------|
| Create Entity | MenuBar | "Create Empty Object" | ✅ |
| Delete Entity | Hierarchy | Delete key / Right-click menu | ✅ |
| Reparent Entity | Hierarchy | Drag-drop | ✅ |
| Unparent to Root | Hierarchy | Drag to root | ✅ |
| Unparent (Menu) | Hierarchy | Right-click → Unparent | ✅ |
| Duplicate Entity | Hierarchy | Right-click → Duplicate | ✅ |
| Duplicate (Shortcut) | Hierarchy | Ctrl+D | ✅ |

## Technical Details

### UID-Based Entity References
Commands store entity UIDs instead of entity IDs because:
- Entity IDs can change when entities are destroyed/recreated
- UIDs remain stable across undo/redo operations
- Prevents dangling entity references

### Transform Preservation
Reparenting operations preserve world transform:
```cpp
// User drags entity to new parent
// World position stays the same
// Local transform adjusts automatically
SetParent(entity, newParent, preserveWorldTransform=true)
```

### Command History
- Maximum 100 undo levels (configurable in Editor.cpp:159)
- Commands are executed immediately, not deferred
- Redo stack is cleared when new command is executed

### Gizmo Integration (3D and 2D)
Transform commands are recorded at the end of gizmo manipulation for both 3D entities and 2D GUI sprites:
```cpp
// When gizmo starts: capture initial transform
if (ImGuizmo::IsUsing() && !m_GizmoWasUsing) {
    m_TransformBeforeGizmo = currentTransform;
}

// When gizmo ends: record command
if (!ImGuizmo::IsUsing() && m_GizmoWasUsing) {
    auto command = std::make_unique<TransformCommand>(...);
    history->Execute(std::move(command));
}
```

**2D Gizmo** (for GUI sprites):
- Uses world matrix for proper hierarchy support
- Converts world matrix back to local space when manipulating
- Fully supports parent-child relationships

### Inspector Property Tracking
Property changes in the Inspector panel are tracked using ImGui's activation/deactivation events:
```cpp
// Checkbox - immediate tracking
if (ImGui::Checkbox("GUI", &sprite.uiOverlay)) {
    auto command = std::make_unique<ComponentPropertyCommand<SpriteComponent>>(...);
    history->Execute(std::move(command));
}

// DragFloat - track on release
ImGui::DragFloat3("Translate", &transform.translate[0], 0.01f);
if (ImGui::IsItemActivated()) {
    m_TransformBeforeEdit = transform;
}
if (ImGui::IsItemDeactivatedAfterEdit()) {
    auto command = std::make_unique<TransformCommand>(...);
    history->Execute(std::move(command));
}
```

## Testing Checklist

### Basic Undo/Redo
- [x] Move entity → Undo → Entity returns to original position
- [x] Create entity → Undo → Entity deleted
- [x] Delete entity → Undo → Entity restored
- [x] Redo works after undo

### Hierarchy Operations
- [x] Reparent entity → Undo → Parent relationship restored
- [x] Reparent entity → World position preserved
- [x] Delete parent → Undo → Parent and children restored
- [x] Unparent (menu) → Undo → Parent relationship restored
- [x] Duplicate entity → Undo → Duplicate deleted
- [x] Duplicate with Ctrl+D → Undo → Duplicate deleted

### Sprite Component Operations
- [x] Toggle GUI checkbox → Undo → Checkbox state restored
- [x] Change sprite texture → Undo → Original texture restored
- [x] Change sprite color → Undo → Original color restored
- [x] Delete entity with sprite → Undo → Sprite component fully restored

### 2D GUI Sprite Hierarchy
- [x] Parent 2D sprite → Child renders at world position
- [x] Move parent → 2D child sprite follows
- [x] 2D gizmo respects parent transforms
- [x] Undo/redo works with parented 2D sprites

### Inspector Transform Edits
- [x] Drag translate → Undo → Position restored
- [x] Drag rotation → Undo → Rotation restored
- [x] Drag scale → Undo → Scale restored

### Complex Workflows
- [x] Create → Move → Delete → Undo 3x → All operations reversed
- [x] Move → Redo → Move applied again
- [x] Multiple entities can be undone independently

### Edge Cases
- [x] Undo with no history does nothing
- [x] Redo after new command clears redo stack
- [x] Entity with children deletes/restores correctly
- [x] Circular parenting prevented by SetParent validation

## Recent Improvements (Session 2)

### SpriteComponent Support
Added full undo/redo support for sprite components:
- **EntitySnapshot** now captures sprite data (textureID, color, uiOverlay)
- Delete/Undo operations preserve sprite components
- Inspector property changes create undo commands

### Inspector Panel Undo Support
Implemented property change tracking for Inspector edits:
- **ComponentPropertyCommand** template for any component type
- Sprite properties (GUI checkbox, texture, color)
- Transform properties (translate, rotate, scale)
- Uses ImGui activation/deactivation events

### 2D Gizmo Undo Support
Added complete undo/redo for 2D GUI sprite manipulation:
- Captures transform before/after gizmo use
- Works identically to 3D gizmo
- Creates "Move 2D", "Rotate 2D", "Scale 2D" commands

### Hierarchy Panel Enhancements
Added missing undo support for hierarchy operations:
- **Unparent menu item** now creates ReparentCommand
- **Duplicate operations** now create DuplicateEntityCommand
- **Ctrl+D shortcut** creates undoable duplicate

### 2D GUI Sprite Hierarchy Fix
Fixed critical bug where GUI sprites ignored parent transforms:
- **Rendering**: GUI sprites now use world transform (respects hierarchy)
- **2D Gizmo**: Uses world matrix like 3D gizmo
- **Parent-child**: 2D sprites now properly follow parent movement

## Future Enhancements

### Possible Extensions
1. **Batch Commands** - Group multiple operations into single undo step
2. **Undo History Panel** - Visual list of all commands with descriptions
3. **Component Add/Remove** - Add/remove components with undo support
4. **More Property Commands** - Extend to all component types
5. **Persistent History** - Save/load undo history with scene files
6. **Undo Branches** - Support multiple undo branches (tree instead of stack)

### Performance Considerations
- Consider limiting snapshot depth for large hierarchies
- Add memory usage tracking for command history
- Implement command coalescing for rapid transform changes

## Integration Notes

### Adding New Commands
To add a new undoable operation:

1. Create command class inheriting from ICommand:
```cpp
class MyCommand : public ICommand {
public:
    void Execute() override { /* ... */ }
    void Undo() override { /* ... */ }
    std::string GetDescription() const override { return "My Operation"; }
};
```

2. Execute the command through CommandHistory:
```cpp
auto* history = m_Owner->GetCommandHistory();
if (history) {
    auto command = std::make_unique<MyCommand>(...);
    history->Execute(std::move(command));
}
```

### Accessing CommandHistory
From any panel:
```cpp
auto* history = m_Owner->GetCommandHistory();
if (history && history->CanUndo()) {
    history->Undo();
}
```

## Architecture Diagram

```
Editor
  └─ CommandHistory (max 100 commands)
       ├─ Command Stack [0..99]
       │    ├─ TransformCommand (3D & 2D)
       │    ├─ CreateEntityCommand
       │    ├─ DeleteEntityCommand (includes SpriteComponent)
       │    ├─ ReparentCommand
       │    ├─ DuplicateEntityCommand
       │    └─ ComponentPropertyCommand<T> (template)
       └─ Current Index (-1 to 99)

Panels (record commands)
  ├─ ViewportPanel → TransformCommand (3D gizmo & 2D gizmo)
  ├─ HierarchyPanel → DeleteEntityCommand, ReparentCommand, DuplicateEntityCommand
  ├─ InspectorPanel → ComponentPropertyCommand<SpriteComponent>
  │                 → TransformCommand (for inspector edits)
  └─ MenuBarPanel → CreateEntityCommand
```

## Compilation Fix Notes

### Fixed Issues
1. **Transform3D not found** - Added `#include "Graphics/Utilities/Data.h"`
2. **Command used after move** - Generate UID in constructor instead of Execute()
3. **Forward declaration issues** - Use full includes where member variables are declared

### Key Includes
- `UndoRedo.h` needs: `ECS/ECS.hpp`, `Graphics/Utilities/Data.h`
- `ViewportPanel.h` needs: `Graphics/Utilities/Data.h` (for Transform3D member)
- Panel .cpp files need: `Commands/UndoRedo.h`

## Conclusion

The undo/redo system is fully integrated and production-ready. It correctly handles:
- ✅ Transform operations with parent-child hierarchies (3D and 2D)
- ✅ Entity lifecycle (create/delete/duplicate)
- ✅ Parent-child relationship changes (reparent/unparent)
- ✅ Component property changes (sprite, transform)
- ✅ Inspector panel edits (all transform and sprite properties)
- ✅ 2D GUI sprite hierarchy support
- ✅ UID-based entity tracking for stability
- ✅ World transform preservation during all operations

The implementation follows established patterns from Unity/Unreal and integrates seamlessly with your existing hierarchy system. All major editor operations are now undoable, including previously missing features like 2D gizmo manipulation, inspector property edits, and duplicate operations.
