[Current over head code view on X](https://x.com/ZacharyGeurts/status/2019211083496513794)  
  
[Demonstration showcase on X](https://x.com/ZacharyGeurts/status/2031298561900446079)  

[Visuals Collection on X](https://x.com/ZacharyGeurts/status/2026807886580417013)  

Diamond Mandelbrot Voxel Mandelbulb in demos.  
<img width="820" alt="Mandelbulb" src="https://github.com/user-attachments/assets/ecb84a68-7ba8-4c2e-aebb-4dc6ea06a7e5" />  

Mandel Toupée - This is our Frosted Demo.  
<img width="820" alt="Mandelbub" src="https://github.com/user-attachments/assets/4c25a25b-5b34-4ae0-b051-b15949f05aa3" />  
https://github.com/ZacharyGeurts/AMOURANTHRTX/blob/main/demos/Windows%203D%20Frosted.zip  
https://github.com/ZacharyGeurts/AMOURANTHRTX/blob/main/demos/Linux%203D%20Frosted.zip  
<img width="820" alt="image" src="https://github.com/user-attachments/assets/8c482464-b5d3-454e-90fc-0b132321afe9" />  
  
Help yourself to the demos folder above for Windows and Linux builds.  
There is a wiki link above that, that I try to keep up to date.  

Navigator/shaders/compute/CANVAS.comp is to be the one file to update.  
Using push constants this one file can manage visuals, input, and audio.  
We permit hot swapping shaders in, so now you have a full engine of whatever you want.  

We pride ourselves with short files.  
They are ~1000 lines each.  
Even Grok "Fast" can return full entire files in one shot.  

Compiling creates one file. Navigator.exe which runs your assets.  
Our toolchain will assist with compiling your shaders and cross compiling.  
Updates require replacing that one file.  
CANVAS.spv will be interchangeable between earlier and later versions.  

We are top down, not down up.  We had triangles at one point.  
The code tends to steer toward getting shorter rather than longer.  

GLSL builds CANVAS .comp to .spv.
If you have a Navigator and an spv file you can run many simple programs.  
If a demo gives you a problem, you can try another Navigator version to play it.  

We are not minimalist. We added every feature from every forum, efficently.  
How efficently? Secure zero cost computing.  
Disney and many researchers contributed to the materials library.  
Grok finds all.  
gcc-14 C++23 Vulkan 1.4+ and SDL3 with inclusion of GLM.  
Requires the VulkanSDK and the cmake works to download and install the rest.  
[VulkanSDK](https://vulkan.lunarg.com/)  
Incremental build system to keep your workflow moving.  

Welcome to the community.  
Releases is the latest stable.  
Use the green code button above to download zip for latest beta.  
Preferred method  to get beta is with git.  
```
git clone https://github.com/ZacharyGeurts/AMOURANTHRTX
cd AMOURANTHRTX
chmod +x linux.sh
./linux.sh --help
```

Untested
```
.\windows.ps1                  # Build Release
.\windows.ps1 -Run             # Build + launch .exe
.\windows.ps1 -Debug           # Build Debug
.\windows.ps1 -Run -Debug      # Debug build + launch
.\windows.ps1 -Clean           # Purge build folder
```

Linux Cross compiling SDK for RTX SECURE ZERO operational cost RTX software development.  
It will have a final form but always be updatable for your projects.  
X11, Wayland, Windows. Adaptable for Metal or Android with toolchains.  

Robust logging system inspired by: [Ellie Fier](https://www.twitch.tv/elliefier)  
We track and tell you everything along the way.  
Filenames, line numbers, function names.  
Debugging becomes a breeze.  

This is RTX. This also is literally AI designing itself.  
We run zero overhead compute. The bottle sealed.  

The security is simple. 4 slot const expr. We store each piece of RTX behind it.  
If a bit of memory changes, and your program is not the one making changes, we exit.  
This is triviality and has "no cost" post compiling.  
We do the same with time by locking our epoch startup time and working off that.  
We secure the input and "camera" as well.  
We gather up the whole of RTX and secure and grant access with simply, rtx() singleton.  
There is no hacking permissable within these walls. Tampering aborts.  
We are the first, last, or another. Game over.  

Development and memes https://x.com/ZacharyGeurts  
I will come off as insane more than rarely if I think it is good for a joke.  
My personal X is not part of this project, but I do share snippets and progress there.  
My personality I try to keep mainly over there and I consider it Pegi 16+.  
Check recent commits to follow code updates.  

We do a 3% profit share of every dollar we can make you.  
I take 1% and 2% goes for the branding to Ammo and Nick.  
If you enjoyed anything from here, send Amouranth a swag bag if you have it, and free copy or Steamlink key.  
She steps on RTX and dominates it now.  

AMOURANTHRTX © 2025 by Zachary Geurts gzac5314@gmail.com  
Free work is licensed under GPL v3.0 or higher.  
Commercial is subject to 3% profit sharing.  
Current and archived content subject to copyright.  

<img width="820" src="https://github.com/ZacharyGeurts/AMOURANTHRTX/blob/main/assets/textures/ammo.png" />  
<img width="820" src="https://github.com/ZacharyGeurts/AMOURANTHRTX/blob/main/media/Screenshot%20from%202025-11-14%2021-05-10.png" />  

- **Why AMOURANTH?** They said they were hiring so I figured I would try to bring in a customer.    
If they do not like my using their brand, they can come to Michigan and stream the firing.  
https://www.twitch.tv/AMOURANTH

<img width="820" alt="Jorts" src="https://github.com/user-attachments/assets/b5c142a7-6ac3-4027-81f2-a1b94d9663d0" />  

**Other work**  

# Universal Equation  🇰🇵 VS 🇺🇸  VS  🇷🇺  VS  🇸🇬  VS  🇨🇳  VS 🇬🇧  VS  🇫🇷  VS  🇲🇽 VS 🇰🇷  VS  🇫🇮  VS  🇯🇵  VS  🇨🇦  VS  🏴󠁧󠁢󠁳󠁣󠁴󠁿  VS  🇦🇺  ...  
**Never condone violence, incorrectness, nor the new Oxford comma.**  

You may find God in the media folder or various screen archery of various projections or whatevery science word Grok knows and I do not.

Welcome friends. God Bless.  
My take? There there is no bottom, side or top to existence.  
Climb inside the blanket and have a look around.  

I think this proves the number 1 is real and 0 is not real.  
Grab the latest release if only interested in the Universal Equation.  
It is a Glorified calculator and you can set it to experimental theory.  
