#include "ui.hpp"
#include <RmlUi/Core/SystemInterface.h>

SystemInterface_GLFW::SystemInterface_GLFW(){
	cursor_pointer = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
	cursor_cross = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
	cursor_text = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
	cursor_move = cursor_pointer;
	cursor_resize = cursor_pointer;
	cursor_unavailable = nullptr;
}

SystemInterface_GLFW::~SystemInterface_GLFW(){
	glfwDestroyCursor(cursor_pointer);
	glfwDestroyCursor(cursor_cross);
	glfwDestroyCursor(cursor_text);
}

void SystemInterface_GLFW::SetWindow(GLFWwindow* in_window){
	window = in_window;
}

double SystemInterface_GLFW::GetElapsedTime(){
	return glfwGetTime();
}

void SystemInterface_GLFW::SetMouseCursor(const Rml::String& cursor_name){
	GLFWcursor* cursor = nullptr;

	if (cursor_name.empty() || cursor_name == "arrow")
		cursor = nullptr;
	else if (cursor_name == "move")
		cursor = cursor_move;
	else if (cursor_name == "pointer")
		cursor = cursor_pointer;
	else if (cursor_name == "resize")
		cursor = cursor_resize;
	else if (cursor_name == "cross")
		cursor = cursor_cross;
	else if (cursor_name == "text")
		cursor = cursor_text;
	else if (cursor_name == "unavailable")
		cursor = cursor_unavailable;
	else if (Rml::StringUtilities::StartsWith(cursor_name, "rmlui-scroll"))
		cursor = cursor_move;

	if (window)
		glfwSetCursor(window, cursor);
}

void SystemInterface_GLFW::SetClipboardText(const Rml::String& text_utf8){
	if (window)
		glfwSetClipboardString(window, text_utf8.c_str());
}

void SystemInterface_GLFW::GetClipboardText(Rml::String& text){
	if (window)
		text = Rml::String(glfwGetClipboardString(window));
}

bool RmlGLFW::ProcessKeyCallback(Rml::Context* context, int key, int action, int mods){
	if (!context)
		return true;

	bool result = true;

	switch (action)
	{
	case GLFW_PRESS:
	case GLFW_REPEAT:
		result = context->ProcessKeyDown(RmlGLFW::ConvertKey(key), RmlGLFW::ConvertKeyModifiers(mods));
		if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
			result &= context->ProcessTextInput('\n');
		break;
	case GLFW_RELEASE: result = context->ProcessKeyUp(RmlGLFW::ConvertKey(key), RmlGLFW::ConvertKeyModifiers(mods)); break;
	}

	return result;
}
bool RmlGLFW::ProcessCharCallback(Rml::Context* context, unsigned int codepoint){
	if (!context)
		return true;

	bool result = context->ProcessTextInput((Rml::Character)codepoint);
	return result;
}

bool RmlGLFW::ProcessCursorEnterCallback(Rml::Context* context, int entered){
	if (!context)
		return true;

	bool result = true;
	if (!entered)
		result = context->ProcessMouseLeave();
	return result;
}

bool RmlGLFW::ProcessCursorPosCallback(Rml::Context* context, GLFWwindow* window, double xpos, double ypos, int mods){
	if (!context)
		return true;

	using Rml::Vector2i;
	using Vector2d = Rml::Vector2<double>;

	Vector2i window_size, framebuffer_size;
	//this things are almost free
	glfwGetWindowSize(window, &window_size.x, &window_size.y);
	glfwGetFramebufferSize(window, &framebuffer_size.x, &framebuffer_size.y);

	// Convert from mouse position in GLFW screen coordinates to framebuffer coordinates (pixels) used by RmlUi.
	const Vector2d mouse_pos = Vector2d(xpos, ypos) * (Vector2d(framebuffer_size) / Vector2d(window_size));
	const Vector2i mouse_pos_round = {int(round(mouse_pos.x)), int(round(mouse_pos.y))};

	bool result = context->ProcessMouseMove(mouse_pos_round.x, mouse_pos_round.y, RmlGLFW::ConvertKeyModifiers(mods));
	return result;
}

bool RmlGLFW::ProcessMouseButtonCallback(Rml::Context* context, int button, int action, int mods){
	if (!context)
		return true;

	bool result = true;

	switch (action)
	{
	case GLFW_PRESS: result = context->ProcessMouseButtonDown(button, RmlGLFW::ConvertKeyModifiers(mods)); break;
	case GLFW_RELEASE: result = context->ProcessMouseButtonUp(button, RmlGLFW::ConvertKeyModifiers(mods)); break;
	}
	return result;
}

bool RmlGLFW::ProcessScrollCallback(Rml::Context* context, double yoffset, int mods){
	if (!context)
		return true;

	bool result = context->ProcessMouseWheel(-float(yoffset), RmlGLFW::ConvertKeyModifiers(mods));
	return result;
}

void RmlGLFW::ProcessFramebufferSizeCallback(Rml::Context* context, int width, int height){
	if (context)
		context->SetDimensions(Rml::Vector2i(width, height));
}

void RmlGLFW::ProcessContentScaleCallback(Rml::Context* context, float xscale){
	if (context)
		context->SetDensityIndependentPixelRatio(xscale);
}

int RmlGLFW::ConvertKeyModifiers(int glfw_mods)
{
	int key_modifier_state = 0;

	if (GLFW_MOD_SHIFT & glfw_mods)
		key_modifier_state |= Rml::Input::KM_SHIFT;

	if (GLFW_MOD_CONTROL & glfw_mods)
		key_modifier_state |= Rml::Input::KM_CTRL;

	if (GLFW_MOD_ALT & glfw_mods)
		key_modifier_state |= Rml::Input::KM_ALT;

	if (GLFW_MOD_CAPS_LOCK & glfw_mods)
		key_modifier_state |= Rml::Input::KM_SCROLLLOCK;

	if (GLFW_MOD_NUM_LOCK & glfw_mods)
		key_modifier_state |= Rml::Input::KM_NUMLOCK;

	return key_modifier_state;
}

Rml::Input::KeyIdentifier RmlGLFW::ConvertKey(int glfw_key){
	switch (glfw_key)
	{
	case GLFW_KEY_A:             return Rml::Input::KI_A;
	case GLFW_KEY_B:             return Rml::Input::KI_B;
	case GLFW_KEY_C:             return Rml::Input::KI_C;
	case GLFW_KEY_D:             return Rml::Input::KI_D;
	case GLFW_KEY_E:             return Rml::Input::KI_E;
	case GLFW_KEY_F:             return Rml::Input::KI_F;
	case GLFW_KEY_G:             return Rml::Input::KI_G;
	case GLFW_KEY_H:             return Rml::Input::KI_H;
	case GLFW_KEY_I:             return Rml::Input::KI_I;
	case GLFW_KEY_J:             return Rml::Input::KI_J;
	case GLFW_KEY_K:             return Rml::Input::KI_K;
	case GLFW_KEY_L:             return Rml::Input::KI_L;
	case GLFW_KEY_M:             return Rml::Input::KI_M;
	case GLFW_KEY_N:             return Rml::Input::KI_N;
	case GLFW_KEY_O:             return Rml::Input::KI_O;
	case GLFW_KEY_P:             return Rml::Input::KI_P;
	case GLFW_KEY_Q:             return Rml::Input::KI_Q;
	case GLFW_KEY_R:             return Rml::Input::KI_R;
	case GLFW_KEY_S:             return Rml::Input::KI_S;
	case GLFW_KEY_T:             return Rml::Input::KI_T;
	case GLFW_KEY_U:             return Rml::Input::KI_U;
	case GLFW_KEY_V:             return Rml::Input::KI_V;
	case GLFW_KEY_W:             return Rml::Input::KI_W;
	case GLFW_KEY_X:             return Rml::Input::KI_X;
	case GLFW_KEY_Y:             return Rml::Input::KI_Y;
	case GLFW_KEY_Z:             return Rml::Input::KI_Z;

	case GLFW_KEY_0:             return Rml::Input::KI_0;
	case GLFW_KEY_1:             return Rml::Input::KI_1;
	case GLFW_KEY_2:             return Rml::Input::KI_2;
	case GLFW_KEY_3:             return Rml::Input::KI_3;
	case GLFW_KEY_4:             return Rml::Input::KI_4;
	case GLFW_KEY_5:             return Rml::Input::KI_5;
	case GLFW_KEY_6:             return Rml::Input::KI_6;
	case GLFW_KEY_7:             return Rml::Input::KI_7;
	case GLFW_KEY_8:             return Rml::Input::KI_8;
	case GLFW_KEY_9:             return Rml::Input::KI_9;

	case GLFW_KEY_BACKSPACE:     return Rml::Input::KI_BACK;
	case GLFW_KEY_TAB:           return Rml::Input::KI_TAB;

	case GLFW_KEY_ENTER:         return Rml::Input::KI_RETURN;

	case GLFW_KEY_PAUSE:         return Rml::Input::KI_PAUSE;
	case GLFW_KEY_CAPS_LOCK:     return Rml::Input::KI_CAPITAL;

	case GLFW_KEY_ESCAPE:        return Rml::Input::KI_ESCAPE;

	case GLFW_KEY_SPACE:         return Rml::Input::KI_SPACE;
	case GLFW_KEY_PAGE_UP:       return Rml::Input::KI_PRIOR;
	case GLFW_KEY_PAGE_DOWN:     return Rml::Input::KI_NEXT;
	case GLFW_KEY_END:           return Rml::Input::KI_END;
	case GLFW_KEY_HOME:          return Rml::Input::KI_HOME;
	case GLFW_KEY_LEFT:          return Rml::Input::KI_LEFT;
	case GLFW_KEY_UP:            return Rml::Input::KI_UP;
	case GLFW_KEY_RIGHT:         return Rml::Input::KI_RIGHT;
	case GLFW_KEY_DOWN:          return Rml::Input::KI_DOWN;
	case GLFW_KEY_PRINT_SCREEN:  return Rml::Input::KI_SNAPSHOT;
	case GLFW_KEY_INSERT:        return Rml::Input::KI_INSERT;
	case GLFW_KEY_DELETE:        return Rml::Input::KI_DELETE;

	case GLFW_KEY_LEFT_SUPER:    return Rml::Input::KI_LWIN;
	case GLFW_KEY_RIGHT_SUPER:   return Rml::Input::KI_RWIN;

	case GLFW_KEY_KP_0:          return Rml::Input::KI_NUMPAD0;
	case GLFW_KEY_KP_1:          return Rml::Input::KI_NUMPAD1;
	case GLFW_KEY_KP_2:          return Rml::Input::KI_NUMPAD2;
	case GLFW_KEY_KP_3:          return Rml::Input::KI_NUMPAD3;
	case GLFW_KEY_KP_4:          return Rml::Input::KI_NUMPAD4;
	case GLFW_KEY_KP_5:          return Rml::Input::KI_NUMPAD5;
	case GLFW_KEY_KP_6:          return Rml::Input::KI_NUMPAD6;
	case GLFW_KEY_KP_7:          return Rml::Input::KI_NUMPAD7;
	case GLFW_KEY_KP_8:          return Rml::Input::KI_NUMPAD8;
	case GLFW_KEY_KP_9:          return Rml::Input::KI_NUMPAD9;
	case GLFW_KEY_KP_ENTER:      return Rml::Input::KI_NUMPADENTER;
	case GLFW_KEY_KP_MULTIPLY:   return Rml::Input::KI_MULTIPLY;
	case GLFW_KEY_KP_ADD:        return Rml::Input::KI_ADD;
	case GLFW_KEY_KP_SUBTRACT:   return Rml::Input::KI_SUBTRACT;
	case GLFW_KEY_KP_DECIMAL:    return Rml::Input::KI_DECIMAL;
	case GLFW_KEY_KP_DIVIDE:     return Rml::Input::KI_DIVIDE;

	case GLFW_KEY_F1:            return Rml::Input::KI_F1;
	case GLFW_KEY_F2:            return Rml::Input::KI_F2;
	case GLFW_KEY_F3:            return Rml::Input::KI_F3;
	case GLFW_KEY_F4:            return Rml::Input::KI_F4;
	case GLFW_KEY_F5:            return Rml::Input::KI_F5;
	case GLFW_KEY_F6:            return Rml::Input::KI_F6;
	case GLFW_KEY_F7:            return Rml::Input::KI_F7;
	case GLFW_KEY_F8:            return Rml::Input::KI_F8;
	case GLFW_KEY_F9:            return Rml::Input::KI_F9;
	case GLFW_KEY_F10:           return Rml::Input::KI_F10;
	case GLFW_KEY_F11:           return Rml::Input::KI_F11;
	case GLFW_KEY_F12:           return Rml::Input::KI_F12;
	case GLFW_KEY_F13:           return Rml::Input::KI_F13;
	case GLFW_KEY_F14:           return Rml::Input::KI_F14;
	case GLFW_KEY_F15:           return Rml::Input::KI_F15;
	case GLFW_KEY_F16:           return Rml::Input::KI_F16;
	case GLFW_KEY_F17:           return Rml::Input::KI_F17;
	case GLFW_KEY_F18:           return Rml::Input::KI_F18;
	case GLFW_KEY_F19:           return Rml::Input::KI_F19;
	case GLFW_KEY_F20:           return Rml::Input::KI_F20;
	case GLFW_KEY_F21:           return Rml::Input::KI_F21;
	case GLFW_KEY_F22:           return Rml::Input::KI_F22;
	case GLFW_KEY_F23:           return Rml::Input::KI_F23;
	case GLFW_KEY_F24:           return Rml::Input::KI_F24;

	case GLFW_KEY_NUM_LOCK:      return Rml::Input::KI_NUMLOCK;
	case GLFW_KEY_SCROLL_LOCK:   return Rml::Input::KI_SCROLL;

	case GLFW_KEY_LEFT_SHIFT:    return Rml::Input::KI_LSHIFT;
	case GLFW_KEY_LEFT_CONTROL:  return Rml::Input::KI_LCONTROL;
	case GLFW_KEY_RIGHT_SHIFT:   return Rml::Input::KI_RSHIFT;
	case GLFW_KEY_RIGHT_CONTROL: return Rml::Input::KI_RCONTROL;
	case GLFW_KEY_MENU:          return Rml::Input::KI_LMENU;

	case GLFW_KEY_KP_EQUAL:      return Rml::Input::KI_OEM_NEC_EQUAL;
	default: break;
	}

	return Rml::Input::KI_UNKNOWN;
}

static Renderer* _renderer;
int   pre_counter_diff = 0;
int  post_counter_diff = 0;
int final_counter_diff = 0;
float upscale_counter_diff = 0;

static void _callback_pre_denoiser_decrease(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	pre_counter_diff--;	
}
static void _callback_pre_denoiser_increase(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	pre_counter_diff++;
}
static void _callback_post_denoiser_decrease(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	post_counter_diff--;
}
static void _callback_post_denoiser_increase(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	post_counter_diff++;
}
static void _callback_final_denoiser_decrease(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	final_counter_diff--;
}
static void _callback_final_denoiser_increase(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	final_counter_diff++;	
}
static void _callback_upscale_decrease(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	upscale_counter_diff -= 0.05;
}
static void _callback_upscale_increase(Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&){
	upscale_counter_diff += 0.05;	
}

static float temp = 109090.4;

bool Ui::SetupDataBinding(Rml::Context* context, Rml::DataModelHandle& my_model){
	Rml::DataModelConstructor constructor = context->CreateDataModel("my_model");
	if (!constructor)
		return false;
	
	// constructor.Bind("button1", &data_bound.if_pressed);
	constructor.BindEventCallback("pre_denoiser_decrease"  ,   _callback_pre_denoiser_decrease);
	constructor.BindEventCallback("pre_denoiser_increase"  ,   _callback_pre_denoiser_increase);
	constructor.BindEventCallback("post_denoiser_decrease" ,  _callback_post_denoiser_decrease);
	constructor.BindEventCallback("post_denoiser_increase" ,  _callback_post_denoiser_increase);
	constructor.BindEventCallback("final_denoiser_decrease", _callback_final_denoiser_decrease);
	constructor.BindEventCallback("final_denoiser_increase", _callback_final_denoiser_increase);
	
	constructor.BindEventCallback("upscaling_ratio_decrease", _callback_upscale_decrease);
	constructor.BindEventCallback("upscaling_ratio_increase", _callback_upscale_increase);

	constructor.Bind("pre_counter", &_renderer->pre_denoiser_count);
	constructor.Bind("post_counter", &_renderer->post_denoiser_count);
	constructor.Bind("final_counter", &_renderer->final_denoiser_count);
	constructor.Bind("upscaling_ratio", &_renderer->_ratio);
	my_model = constructor.GetModelHandle();
	

	return true;
}
static int mouse_down_state = -1;
static int mouse_up_state = -1;
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        mouse_down_state = 0;
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        mouse_down_state = 1;
	if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
        mouse_down_state = 2;
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        mouse_up_state = 0;
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
        mouse_up_state = 1;
	if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
        mouse_up_state = 2;
}

void Ui::setup(){
	_renderer = renderer;
	
	sysInterface.SetWindow(renderer->window.pointer);
    Rml::SetRenderInterface(renderer->ui_render_interface);
    Rml::SetSystemInterface(&sysInterface);
	sysInterface.SetWindow(renderer->window.pointer);
    Rml::Initialise();

    assert(renderer->swapChainExtent.width != 0 && renderer->swapChainExtent.height != 0);
    context = Rml::CreateContext("main", Rml::Vector2i(renderer->swapChainExtent.width, renderer->swapChainExtent.height));

    Rml::Debugger::Initialise(context);
    Rml::LoadFontFace("assets/LatoLatin-Regular.ttf");
	SetupDataBinding(context, my_model);

	document = context->LoadDocument("assets/simple.rml");
	if (!document){
		Rml::Shutdown();
			}

	document->Show();

	glfwSetMouseButtonCallback(renderer->window.pointer, mouse_button_callback);
}

void Ui::update(){
	if(renderer->ui_render_interface->default_image == NULL){
		byte def_img_src[4] = {(byte)255,(byte)255,(byte)255,(byte)255};
		bool res = renderer->ui_render_interface->GenerateTexture((Rml::TextureHandle&)(renderer->ui_render_interface->default_image), (Rml::byte*)&def_img_src, (Rml::Vector2i){1,1});
		assert(res);
	}
	double xpos, ypos;
	glfwGetCursorPos(renderer->window.pointer, &xpos, &ypos);
	context->ProcessMouseMove(int(floor(xpos)), int(floor(ypos)), 0);
	
	if(mouse_down_state != -1){
		// printl(mouse_down_state)
		context->ProcessMouseButtonDown(mouse_down_state, 0);
		mouse_down_state = -1;
	}
	if(mouse_up_state != -1){
		// printl(mouse_up_state)
		context->ProcessMouseButtonUp(mouse_up_state, 0);
		mouse_up_state = -1;
	}
	
	// my_model.DirtyVariable("pre_counter");

	if(  pre_counter_diff != 0){
		renderer->pre_denoiser_count += pre_counter_diff;
		my_model.DirtyVariable("pre_counter");
		pre_counter_diff = 0;
	}
	if( post_counter_diff != 0){
		renderer->post_denoiser_count += post_counter_diff;
		my_model.DirtyVariable("post_counter");
		post_counter_diff = 0;
	}
	if(final_counter_diff != 0){
		renderer->final_denoiser_count += final_counter_diff;
		my_model.DirtyVariable("final_counter");
		final_counter_diff = 0;
	}
	if(upscale_counter_diff != 0){
		renderer->_ratio += upscale_counter_diff;
		renderer->_ratio = glm::clamp(renderer->_ratio, 1.0f, 3.0f);
		// renderer->resized = true;

		my_model.DirtyVariable("upscaling_ratio");
		upscale_counter_diff = 0;
	}

	context->Update();
}

void Ui::draw(){
	context->Render();
}

void Ui::cleanup(){
	document->Close();
	// Rml::ReleaseTextures();
	// Rml::ReleaseFontResources();
	// Rml::ReleaseCompiledGeometry();
	// Rml::ReleaseMemoryPools();
	Rml::Shutdown();
	renderer->ui_render_interface->ReleaseTexture((Rml::TextureHandle)renderer->ui_render_interface->default_image);
		// context.
}