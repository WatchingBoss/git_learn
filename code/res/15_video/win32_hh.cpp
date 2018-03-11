/* ===========================================================
	$File: $
	$Date: $
	$Revision: $
	$Creator: Egor Smirnov (Santa) $
	$Notice: $
   ===========================================================*/

/*
 * TODO(santa): THIS IS NOT A FINAL PLATFORM LAYER!

	- Saved game locations
	- Getting a handle to our own executeble file
	- Asset loading path
	- Threading (launch a thread)
	- Raw Input (support for multiple keyboards)
	- Sleep/timeBeginPeriod
	- ClipCurser() (for multimonitor support)
	- Fullscreen support
	- WM_SETCURSOR (control cursor visibility)
	- QueryCancelAutoplay
	- Blit speed improvements (BitBit)
	- Hardware acceleration (OpenGL or Direct3D or BOTH?)
	- GetKeyboardLayout (for French keybords, international WASD support)

	Just a partial list of stuff!	
*/

// TODO(santa): Implement sine ourselves
#include <math.h>
#include <stdint.h>

#define internal        static 
#define local_persist   static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32   bool32;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float  real32;
typedef double real64;

#include "hh.h"
#include "hh.cpp"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_hh.h"

// TODO(santa): This is a global for now.
global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// NOTE(santa): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(santa): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
{
	debug_read_file_result Result = {};

	HANDLE FileHandle =  CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			if (Result.Contents)
			{
				DWORD BytesRead;
				if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
				   (FileSize32 == BytesRead))
				{
					// NOTE(santa): File read successfully
					Result.ContentsSize = FileSize32;
				}
				else
				{
					// TODO(santa): Logging
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				// TODO(santa): Logging
			}
		}
		else
		{
			// TODO(santa): Logging
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO(santa): Logging
	}
	return(Result);
}

internal void
DEBUGPlatformFreeFileMemory(void *Memory)
{
	if(Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE);
	}
}

internal bool32
DEBUGPlatformWrittenEntireFile(char *Filename, uint32 MemorySize, void *Memory)
{	
	bool32 Result = false;

	HANDLE FileHandle =  CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if(ReadFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			// NOTE(santa): File read successfully
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			// TODO(santa): Logging
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// TODO(santa): Logging
	}
	return(Result);
}

internal void
Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if(!XInputLibrary)
	{
		// TODO(santa): Diagnostic
		HMODULE XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}
  
	if(!XInputLibrary)
	{
		// TODO(santa): Diagnostic
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}
	if(XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState) {XInputGetState = XInputGetStateStub;}
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState) {XInputSetState = XInputSetStateStub;}

		// TODO(santa): Diagnostic
	}
	else
	{
		// TODO(santa): Diagnostic
	}
}

internal void
Win32InitDSound(HWND Window,int32 SamplesPerSecond, int32 BufferSize)
{
	// NOTE(santa): Load the library
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if(DSoundLibrary)
	{
		// NOTE(santa): Get a DirectSound object! - cooperative
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
			GetProcAddress(DSoundLibrary, "DirectSoundCreate");
	    LPDIRECTSOUND DirectSound;
		if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;
					
			if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// NOTE(sants): "Create" a primary buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
			    if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(Error))
					{
						// NOTE(sants): We have finally set the format!
						OutputDebugStringA("Primaty buffer format was set\n");
					}
					else
					{
					}
				}
				else
				{
					// TODO(santa): Diagnostic 	
				}
			}
			else
			{
				// TODO(santa): Diagnostic 	
			}

			// TODO(santa): DSBCAPS_GETCURRENTPOSITION2	
			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			HRESULT Error =
				DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0); 
			if(SUCCEEDED(Error))
			{
				OutputDebugStringA("SecondaryPrimaty buffer created successfully\n");
				// NOTE(sants): Start it playing!
			}
		}
		else
		{
			// TODO(santa): Diagnostic
		}
	}
	else
	{
		// TODO(santa): Diagnostic
	}
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
    Result.Width  = ClientRect.right  - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
	
	return(Result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	//TODO(santa): Bulletproof this.

	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

    Buffer->Width     = Width;
    Buffer->Height    = Height;
	int BytesPerPixel = 4;

	// NOTE(santa): When the biHeight field is negative, this is the clue to
	// Windows to treat this bitmap as top-down, not bottom-up, meaning that
	// the first three bytes of the image are the color for the top left pixel
	// int the bitmap, not the bottom left!
	Buffer->Info.bmiHeader.biSize        = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth       = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight      = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes      = 1;
	Buffer->Info.bmiHeader.biBitCount    = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * BytesPerPixel;

	// TODO(santa): Probably clear this to black
}	
internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
						   HDC DeviceContext,int WindowWidth, int WindowHeight)
{
	// TODO(santa): Aspect ratio correction
	// TODO(santa): Play with stretch modes
	StretchDIBits(DeviceContext,
				  /*
					X, Y, Width, Height,
					X, Y, Width, Height,
				  */
				  0, 0, WindowWidth, WindowHeight,
				  0, 0, Buffer->Width, Buffer->Height,
				  Buffer->Memory,
				  &Buffer->Info,
				  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND   Window,
						UINT   Message,
						WPARAM WParam,
						LPARAM LParam)
{
	LRESULT Result = 0;
	
	switch(Message)
	{		
		case WM_DESTROY:
		{
			// TODO(santa): Handle this as an error - recreate window?
			GlobalRunning = false;
		} break;
		
		case WM_CLOSE:
		{
			// TODO(santa): Handle this with a message to the user?
			GlobalRunning = false;
		} break;
		
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 VKCode = WParam;
			bool32 WasDown = ((LParam & (1 << 30)) != 0);
			bool32 IsDown = ((LParam & (1 << 31)) == 0);

			if(WasDown != IsDown)
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
				else if (VKCode == 'Q')
				{

				}
				else if (VKCode == 'E')
				{

				}
				else if (VKCode == VK_UP)
				{

				}
				else if (VKCode == VK_LEFT)
				{

				}
				else if (VKCode == VK_DOWN)
				{

				}
				else if (VKCode == VK_RIGHT)
				{

				}
				else if (VKCode == VK_ESCAPE)
				{
					OutputDebugStringA("ESCAPE: ");
					if(IsDown)
					{
						OutputDebugStringA("IsDown");
					}
					if(WasDown)
					{
						OutputDebugStringA("WasDown");
					}
					OutputDebugStringA("\n");
				}
				else if (VKCode == VK_SPACE)
				{
				}
			}

			bool32 AltKeyWasDown = (LParam & (1 << 29));
			if ((VKCode == VK_F4) && AltKeyWasDown)
			{
				GlobalRunning = false;
			}
		} break;

		
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width  = Paint.rcPaint.right  - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			
			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
									   Dimension.Width, Dimension.Height);
			EndPaint(Window, &Paint);
		} break;
		
		default:
		{
//			OutputDebugStringA("WM_default\n");
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}
	
	return(Result);
}

internal void
Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
											 &Region1, &Region1Size,
											 &Region2, &Region2Size,
											 0)));
	{
		// TODO(santa): assert that Region1Size/Region2Size is valid
		uint8 *DestSample = (uint8 *)Region1;
		for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
		{
			*DestSample++ = 0;
    	}
		
		DestSample = (uint8 *)Region2;
		for(DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
		{
			*DestSample++ = 0;
    	}
		
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
					 game_sound_output_buffer *SourceBuffer)
{
	// TODO(santa): More strenuous test!
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
											 &Region1, &Region1Size,
											 &Region2, &Region2Size,
											 0)));
	{
		// TODO(santa): assert that Region1Size/Region2Size is valid

		// TODO(sants): Collapse these two loops
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;
		for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			++SoundOutput->RunningSampleIndex;
		}
						
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
								game_button_state *OldState, DWORD ButtonBit,
								game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

int CALLBACK
WinMain(HINSTANCE Instance,
	    HINSTANCE PrevInstance,
		LPSTR     CommandLine,
		int       ShowCode)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

	Win32LoadXInput();
	
 	WNDCLASS WindowClass = {};
	
	Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
	 
	WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
 	WindowClass.lpfnWndProc = Win32MainWindowCallback;
 	WindowClass.hInstance = Instance;
	// WindowClass.hIcon;
    WindowClass.lpszClassName = "HHWindowClass";
	
	if (RegisterClass(&WindowClass))
	{
		HWND Window =
			CreateWindowExA(0,
						   WindowClass.lpszClassName,
						   "Copy handmade hero",
						   WS_OVERLAPPEDWINDOW|WS_VISIBLE,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   CW_USEDEFAULT,
						   0,
						   0,
						   Instance,
			    		   0); 
		if (Window)
		{
			// NOTE(santa): Since we specified CS_OWNDC, we can just
			// get one device context and use it forever because we
			// are not sharing it with anyone.
			HDC DeviceContext = GetDC(Window);

			win32_sound_output SoundOutput = {};

			// TODO(santa): Make this like sixty seconds?
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.BytesPerSample = sizeof(int16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;

			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
												   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HH_INTERNAL
			LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
			LPVOID BaseAddress = 0;
#endif
			
			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TransientStorageSize = Gigabytes(4);

			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
			GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize,
													   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage +
										   GameMemory.PermanentStorageSize);

			if(Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
			{
				game_input Input[2] = {};
				game_input *NewInput = &Input[0];
				game_input *OldInput = &Input[1];
			
				LARGE_INTEGER LastCounter;
				QueryPerformanceCounter(&LastCounter);
				uint64 LastCycleCount = __rdtsc(); 
				while(GlobalRunning)
				{				
					MSG Message;

					while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
					{
						if (Message.message == WM_QUIT)
						{
							GlobalRunning = false;
						}
					
						TranslateMessage(&Message);
						DispatchMessage(&Message);
					}

					// TODO(santa): Should we poll this more frequently
					int MaxControllerCount = XUSER_MAX_COUNT;
					if(MaxControllerCount > ArrayCount(NewInput->Controllers))
					{
						MaxControllerCount = ArrayCount(NewInput->Controllers); 	
					}
				
					for (DWORD ControllerIndex = 0;
						 ControllerIndex < MaxControllerCount;
						 ++ControllerIndex)
					{
						game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
						game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];
					
						XINPUT_STATE ControllerState;
						if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{
							// NOTE(santa): This controller is plugged in
							// TODO(santa): See if ControllerState.dwPacketNumber increments too rapidly
							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

							// TODO(santa): DPad
							bool32 Up            = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
							bool32 Down          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
							bool32 Left          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
							bool32 Right         = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

							NewController->IsAnalog = true;
							NewController->StartX = OldController->EndX;
							NewController->StartY = OldController->EndY;

							// TODO(santa): Dead zone processing!
							// XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 
							// XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE

							// TODO(santa): Min/Max macros!!!
							real32 X;
							if(Pad->sThumbLX < 0)
							{
								X = (real32)Pad->sThumbLX / 32768.0f;
							}
							else
							{
								X = (real32)Pad->sThumbLX / 32768.0f;
							}
							NewController->MinX = NewController->MaxX = NewController->EndX = X;

							real32 Y;
							if(Pad->sThumbLY < 0)
							{
								Y = (real32)Pad->sThumbLY / 32768.0f;
							}
							else
							{
								Y = (real32)Pad->sThumbLY / 32768.0f;
							}
							NewController->MinY = NewController->MaxY = NewController->EndY = Y;
					    
							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->Down,
															XINPUT_GAMEPAD_A,
															&NewController->Down);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->Right,
															XINPUT_GAMEPAD_B,
															&NewController->Right);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->Left,
															XINPUT_GAMEPAD_X,
															&NewController->Left);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->Up,
															XINPUT_GAMEPAD_Y,
															&NewController->Up);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->LeftShoulder,
															XINPUT_GAMEPAD_LEFT_SHOULDER,
															&NewController->LeftShoulder);
							Win32ProcessXInputDigitalButton(Pad->wButtons,
															&OldController->RightShoulder,
															XINPUT_GAMEPAD_RIGHT_SHOULDER,
															&NewController->RightShoulder);
	
//						bool32 Start         = (Pad->wButtons & XINPUT_GAMEPAD_START);
//						bool32 Back          = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						
						}			 
						else
						{
							// NOTE(santa): The controller is not available
						}
					}

					DWORD ByteToLock    = 0;
					DWORD TargetCursor  = 0;
					DWORD BytesToWrite  = 0;
					DWORD PlayCursor    = 0;
					DWORD WriteCursor   = 0;
					bool32 SoundIsValid = false;
					// TODO(santa): Tighten up sound so that we know where we should be
					// writing to and can anticipate the time spent in the game update.
					if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
					{
						ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
									  SoundOutput.SecondaryBufferSize);
						TargetCursor =
							((PlayCursor +
							  (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
							 SoundOutput.SecondaryBufferSize); 
						if(ByteToLock > TargetCursor)
						{
							BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}

						SoundIsValid = true;
					}

					game_sound_output_buffer SoundBuffer = {};
					SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
					SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;;
					SoundBuffer.Samples = Samples;
				
					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackbuffer.Memory;
					Buffer.Width  = GlobalBackbuffer.Width;
					Buffer.Height = GlobalBackbuffer.Height;
					Buffer.Pitch  = GlobalBackbuffer.Pitch;
					GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

					// NOTE(santa): DirectSound output test
					if(SoundIsValid)
					{					
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

					win32_window_dimension Dimension = Win32GetWindowDimension(Window);
					Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
											   Dimension.Width, Dimension.Height);

					uint64 EndCycleCount = __rdtsc(); 

					LARGE_INTEGER EndCounter;
					QueryPerformanceCounter(&EndCounter);

					// TODO(santa): Display the value here
					uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
					real64 MSPerFrame = ((1000.0f * (real64)CounterElapsed) / (real64)PerfCountFrequency);
					real64 FPS = (real64)PerfCountFrequency / (real64)CounterElapsed;
					real64 MCPF = (real64)(CyclesElapsed / 1000000.0f);

#if 0
					char Buffer1[256];
					sprintf(Buffer1, "%.2fms/f, %.2ff/s, %.2fmc/f\n", MSPerFrame, FPS, MCPF);
					OutputDebugStringA(Buffer1);
#endif
				
					LastCounter = EndCounter;
					LastCycleCount = EndCycleCount;

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;
					// TODO(santa): Should I clear these here?
				}
			}
			else
			{
				// TODO(santa): Logging
			}
		}
		else
		{
			// TODO(santa): Logging
		}
	}
	else
	{
		// TODO(santa): Logging
	}
	 
    return(0);
}

