using System;
internal static class LocaleTest {
    static int Main() {
        int[] ids={0x409,0x809,0x1009,0x40a,0xc0a,0x80a,0x410,0x810,0x40c,0x80c,0xc0c,0x407,0x807,0xc07,0x411,0x416,0};
        int[] languages={0,0,0,1,1,1,2,2,3,3,3,4,4,4,0,0,0};
        int failures=0;
        for(int i=0;i<ids.Length;i++) if(LocaleText.LanguageForId(ids[i])!=languages[i]) failures++;
        foreach(var entry in LocaleText.Entries) {
            if(entry.Value.Length!=5) failures++;
            foreach(string value in entry.Value) if(String.IsNullOrWhiteSpace(value)) failures++;
        }
        Console.WriteLine("INSTALLER LOCALE RESULT: {0} ({1} failures)",failures==0?"PASS":"FAIL",failures);
        return failures==0?0:1;
    }
}
