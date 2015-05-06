

#include <iostream>
#include <Windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>

using namespace std;

#define internal static
#define local_persist static
#define global_variable static

typedef int64_t int64;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;
typedef int32 bool32;

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

//TODO: This is global for now
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;


struct win32_window_dimension
{
	int Width;
	int Height;
};

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;
	return (Result);
}


//XINPUT GET STATE
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex,  XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//XINPUT SET STATE
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

//Sound
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuiDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void 
Win32LoadXInput()
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState){ XInputGetState = XInputGetStateStub; }
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState){ XInputSetState = XInputSetStateStub; }
	}
	else
	{
		//TODO DIAGNOSTICS
	}
}





internal void
Win32InitDSound(HWND Window, int32 BufferSize, int32 SamplesPerSecond)
{
	
	//Load Library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
	if (DSoundLibrary)
	{

		//Get DirectSound Object
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		//TODO Check if works on XP,
		LPDIRECTSOUND DirectSound;

		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			
			//Set wave Format
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = WaveFormat.nChannels*WaveFormat.wBitsPerSample / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				// "Create" a primary buffer
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = { sizeof(BufferDescription) };
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					//TODO DSBCAPS_GLOBALFOCUS?
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						OutputDebugStringA("PRIMARY BUFFER SET\n");
					}
					else
					{
						//TODO Diagnostics
					}
				}
				else
				{
					//TODO DIAGNOSTICS
				}
			}
			else
			{
				//TODO Diagnostics
			}

			//Create Secondary Buffer
			//TODO : DBSCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = { sizeof(BufferDescription) };
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				OutputDebugStringA("SEONDARY BUFFER SET\n");
			}
			else
			{
				//TODO DIAGNOSTICS
			}
			
		}
		//BufferDescription.dwBufferBytes = BufferSize;
		//start it playing
		else
		{
			//TODO DIAGNOSTICS
		}
	}
	else
	{
		//TODO DIAGNOSTICS
	}
}

internal void
RenderWeirdGradient(win32_offscreen_buffer Buffer, int BlueOffset, int GreenOffset)
{
	//TODO: See what optimizer does	
	uint8 * Row = (uint8*)Buffer.Memory;
	for (int Y = 0; Y < Buffer.Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer.Width; ++X)
		{
			uint8 Red = 0;
			uint8 Green = (Y+ GreenOffset);
			uint8 Blue = (X + BlueOffset);
			uint8 Pad = 0;
			/* Switch for Big Endian Machine   BB GG RR XX*/
			*Pixel++ = ((Green << 8) | Blue);

		}
		Row += Buffer.Pitch;
	}

}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;

	//NOTE: Negative Height is a cue to windows that this is a top-down buffer, as opposed to bottom-up
	//the first 3 bytes are the color of the top-left pixel
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	//
	int BitmapMemorySize = (Buffer->Width*Buffer->Height)*Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	
	
	//TODO Probably clear this to black
	Buffer->Pitch = Width * Buffer->BytesPerPixel;
	
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer Buffer,  HDC DeviceContext, int WindowWidth, int WindowHeight, int X, int Y, int Width, int Height)
{
	//TODO: Fix Aspect Ratio Correction
	//TODO: Play with stretch modes

	 StretchDIBits(DeviceContext,
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer.Width, Buffer.Height,
		Buffer.Memory,
		&Buffer.Info,
		DIB_RGB_COLORS, SRCCOPY
		);
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND Window,UINT Message,WPARAM WParam,LPARAM LParam)
{
	LRESULT Result =0 ;


	switch (Message)
	{
		case WM_SIZE:
		{
			/*
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32ResizeDIBSection(&GlobalBackBuffer, Dimension.Width, Dimension.Height);
			*/
		}break;

		case WM_DESTROY:
		{
			//TODO Handlw with message 
			GlobalRunning = false;
			OutputDebugStringA(" WM_DESTROY\n");
		}break;

		case WM_CLOSE:
		{
			//TODO Handle recreate window
			GlobalRunning = false;
			OutputDebugStringA(" WM_CLOSE\n");
		}break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA(" WM_ACTIVATEAPP\n");
		}break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);

			Win32DisplayBufferInWindow(GlobalBackBuffer,  DeviceContext, Dimension.Width, Dimension.Height, X, Y, Width, Height);
			//PatBlt(DeviceContext,X, Y, Width, Height, WHITENESS);

			EndPaint(Window, &Paint);
		}break;

		//Key handling
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = WParam;
			bool WasDown = ((LParam & (1 << 30)) !=0);
			bool IsDown = ((LParam & (1 << 31)) == 0);
			if (WasDown != IsDown)
			{
				if (VKCode == 'W')
				{
				}
				else if (VKCode == 'A')
				{
				}
				else if (VKCode == 'S')
				{
				}
				else if (VKCode == 'D')
				{
				}
				else if (VKCode == VK_UP)
				{
				}
				else if (VKCode == VK_DOWN)
				{
				}
				else if (VKCode == VK_LEFT)
				{
				}
				else if (VKCode == VK_RIGHT)
				{
				}
				else if (VKCode == 'Q')
				{
				}
				else if (VKCode == 'E')
				{
				}
				else if (VKCode == VK_SPACE)
				{
				}
				else if (VKCode == VK_ESCAPE)
				{
				}
			}
			/// ALT + F4
			if (VKCode == VK_F4 && ((LParam &(1 << 29)) !=0))
			{
				GlobalRunning = false;
			}
		}break;

		default:
		{
			//OutputDebugStringA(" LRESULT CALLBACK: Nothing\n");
			Result = DefWindowProc(Window, Message, WParam, LParam);
		}break;
	}

	return (Result);
}



int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int showcode)
{
	Win32LoadXInput();
	
	//sound test
	//DirectSound outut test
	int SamplesPerSecond = 48000;
	int ToneHz = 250;
	int16 ToneVolume = 800;
	uint32 RunningSampleIndex = 0;
	int SquareWavePeriod = SamplesPerSecond / ToneHz;
	int HalfSquareWavePeriod = SquareWavePeriod/ 2;
	int BytesPerSample = sizeof(int16) * 2;
	int SecondaryBufferSize = SamplesPerSecond*BytesPerSample;


	//TODO : Check if Redraw/Vreraw/OwnDC still matter
	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer,1200, 720);

	WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
//	WindowClass.hIcon;
//	WindowClass.lpszMenuName;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowExA(
			 0,
			 WindowClass.lpszClassName,
			 "Handmade Hero",
			 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			 CW_USEDEFAULT,
			 CW_USEDEFAULT,
			 CW_USEDEFAULT,
			 CW_USEDEFAULT,
			 0,
			 0,
			 Instance,
			 0
			);
		if (Window)
		{
			HDC DeviceContext = GetDC(Window);
			GlobalRunning = true;
			 
			int XOffset = 0;
			int YOffset = 0;

			//sound
			Win32InitDSound(Window, 48000, 48000*sizeof(int16)*2);

			//Tessting sound
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			while (GlobalRunning)
			{
				
				MSG Message;
				while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{ 
					if (Message.message == WM_QUIT)
					{
						GlobalRunning = FALSE;
					}
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
					
				}

				//INPUT
				//TODO: Should this be polled more often?
				for (DWORD ControllerIndex =0; ControllerIndex < XUSER_MAX_COUNT;ControllerIndex++)
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						//plugged in
						//TODO See if ControllerState.dwPacketNumber incremens too rapidly
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

						BOOL Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						BOOL Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						BOOL Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						BOOL Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						BOOL Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
						BOOL Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						BOOL LeftThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
						BOOL RightThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
						BOOL LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						BOOL RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						BOOL XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
						BOOL YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
						BOOL AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
						BOOL BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);

						int16 StickX = Pad->sThumbLX;
						int16 StickY = Pad->sThumbLY;

						XOffset -= (StickX >>12);
						YOffset += (StickY>>12);

					}
					else
					{
						//not available
					}
				}
				

				RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);

				DWORD PlayCursor;
				DWORD WriteCursor;
				if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
				{

					DWORD BytesToLock = RunningSampleIndex*BytesPerSample % SecondaryBufferSize;
					DWORD BytesToWrite;
					if (BytesToLock == PlayCursor)
					{
						BytesToWrite = SecondaryBufferSize;
					}
					else if(BytesToLock > PlayCursor)
					{
						BytesToWrite = (SecondaryBufferSize - BytesToLock) + PlayCursor;
					}
					else
					{
						BytesToWrite = PlayCursor - BytesToLock;
					}
					
					VOID *Region1;
					DWORD Region1Size;
					VOID *Region2;
					DWORD Region2Size;
					
					if (SUCCEEDED(GlobalSecondaryBuffer->Lock(BytesToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
					{
						//TODO asset region1size/Regon2size valid
						int16 *SampleOut = (int16 *)Region1;
						DWORD Region1SampleCount = Region1Size / BytesPerSample;
						



						for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++)
						{
							int16 SampleValue = ((RunningSampleIndex / HalfSquareWavePeriod)%2) ? ToneVolume : -ToneVolume;
							*SampleOut++ = SampleValue;
							*SampleOut++ = SampleValue;
							++RunningSampleIndex;
						}

						DWORD Region2SampleCount = Region2Size / BytesPerSample;
						for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
						{
							int16 SampleValue = ((RunningSampleIndex / HalfSquareWavePeriod) % 2) ? ToneVolume: -ToneVolume;
							*SampleOut++ = SampleValue;
							*SampleOut++ = SampleValue;
							++RunningSampleIndex;

						}
					}

					GlobalSecondaryBuffer->Unlock(&Region1, Region1Size, &Region2, Region2Size);

				}
				
				//TODO get rid of this
				
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);
				Win32DisplayBufferInWindow(GlobalBackBuffer, DeviceContext, Dimension.Width, Dimension.Height, 0, 0, Dimension.Width, Dimension.Height);
				ReleaseDC(Window, DeviceContext);
				XOffset++;
				YOffset++;
				////////
			}
		}
		else
		{
			
		}
		//TODO : Logging Failure
		
	}
	else
	{
		//TODO :Logging atom return  failure (1)

	}


	return (0);
}
