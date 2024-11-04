### Lum::InputHandler

This is a basic input handling system built on top of GLFW. It allows you to define custom actions for your game using an enumeration, specify how these actions are triggered, and map them to various input sources such as keyboard keys, mouse buttons, and gamepad buttons.

#### Features 
 
- **Indirect Input** : Create your own enumeration to define actions (e.g., `Jump`, `Shoot`), define callbacks for them and then (re)map keys to actions
 
- **Action Types** : Specify how actions are triggered: 
  - **One-shot** : Triggered once when the input is pressed.
 
  - **Continuous** : Triggered every frame while the input is held down.
 

#### Usage 
 
1. **Create an Enum for Actions** : Define your actions using an enumeration.

```cpp
enum class GameAction {
    Jump,
    Shoot,
    LAST_ACTION // Always keep this as the last element
};
```
 
2. **Instantiate the InputHandler** : Create an instance of `Lum::InputHandler` with your action enum

```cpp
Lum::InputHandler<GameAction> inputHandler;
```
 
3. **Set Up Callbacks** : Call the `setup()` initialize GLFW callbacks

```cpp
inputHandler.setup(window);
```
 
4. **Poll Updates** : In your main game loop, call `pollUpdates()` to process input

```cpp
while (!glfwWindowShouldClose(window)) {
    inputHandler.pollUpdates();
    // Other game logic...
}
```
 
5. **Define Actions** : Bind keys, mouse buttons, and gamepad buttons to actions, set action types, and assign callbacks as needed.

```cpp
inputHandler.rebindKey(GameAction::Jump, GLFW_KEY_SPACE);
inputHandler.setActionType(GameAction::Jump, ActionType::OneShot);
inputHandler.setActionCallback(GameAction::Jump, [](GameAction action) {
    // Handle jump action...
});
```

#### TODO 
 
- `InputState` enum to manage different input contexts (like Esc in StateGame opens menu, but Esc in StateMenu closes menu)