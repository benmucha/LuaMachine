// Copyright 2018-2023 - Roberto De Ioris

using UnrealBuildTool;

public class LuaMachine : ModuleRules
{
    public LuaMachine(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        bUseUnity = false;

        PublicIncludePaths.AddRange(
            new string[] {
				// ... add public include paths required here ...
			}
            );


        PrivateIncludePaths.AddRange(
            new string[] {
				// ... add other private include paths required here ...
			}
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "HTTP",
                "Json",
                "PakFile"
				// ... add other public dependencies that you statically link with here ...
			}
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "UMG",
                "InputCore",
				// ... add private dependencies that you statically link with here ...	
			}
            );


        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
				// ... add any modules that your module loads dynamically here ...
			}
            );

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]{
                "UnrealEd",
                "Projects"
            });
        }

        string ThirdPartyDirectory = System.IO.Path.Combine(ModuleDirectory, "..", "ThirdParty");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyDirectory, "x64", "liblua53_win64.lib"));
        }

        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyDirectory, "x64", "liblua53_mac.a"));
        }

        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyDirectory, "x64", "liblua53_linux64.a"));
        }

        else if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyDirectory, "ARM64", "liblua53_linux_aarch64.a"));
        }

        else if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyDirectory, "ARMv7", "liblua53_android.a"));
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyDirectory, "ARM64", "liblua53_android64.a"));
        }

        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicAdditionalLibraries.Add(System.IO.Path.Combine(ThirdPartyDirectory, "ARM64", "liblua53_ios.a"));
        }

    }
}


/*
if (VMType == LuaVMType.LuaJIT)
{
    PublicDefinitions.Add("LUAMACHINE_LUAJIT=1");
    if (Target.Platform == UnrealTargetPlatform.Win64)
    {
        string ThirdParty = System.IO.Path.Combine(ModuleDirectory, "..", "ThirdParty");
        string ThirdPartyX64 = System.IO.Path.Combine(ThirdParty, "x64");

        // Accept either file name (your lib is named luajit_win64.lib)
        string LibA = System.IO.Path.Combine(ThirdPartyX64, "luajit_win64.lib");
        string LibB = System.IO.Path.Combine(ThirdPartyX64, "lua51.lib");
        if (System.IO.File.Exists(LibA))
            PublicAdditionalLibraries.Add(LibA);
        else if (System.IO.File.Exists(LibB))
            PublicAdditionalLibraries.Add(LibB);
        else
            throw new BuildException($"LuaJIT import lib not found: {LibA} or {LibB}");

        // At runtime the MSVC LuaJIT build loads lua51.dll by default.
        // Some setups rename it to luajit_win64.dll. Delay-load both to be safe.
        PublicDelayLoadDLLs.Add("lua51.dll");
        PublicDelayLoadDLLs.Add("luajit_win64.dll");

        // Stage/copy the DLL(s) next to the built module automatically.
        // We point BOTH target names at the same source file on disk so either name works.
        string DllSource =
            System.IO.File.Exists(System.IO.Path.Combine(ThirdPartyX64, "lua51.dll"))
                ? System.IO.Path.Combine(ThirdPartyX64, "lua51.dll")
                : System.IO.Path.Combine(ThirdPartyX64, "luajit_win64.dll");

        if (!System.IO.File.Exists(DllSource))
            throw new BuildException($"LuaJIT DLL not found in {ThirdPartyX64} (expected lua51.dll or luajit_win64.dll)");

        // Copy to the output dir (Editor/Development, Game, etc.)
        RuntimeDependencies.Add("$(TargetOutputDir)/lua51.dll",       DllSource);
        RuntimeDependencies.Add("$(TargetOutputDir)/luajit_win64.dll", DllSource);
    }
}
else
{
    PublicDefinitions.Add("LUAMACHINE_LUAJIT=0");
}
*/