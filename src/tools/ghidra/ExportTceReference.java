// Export a reference listing; this is decompiler output, not compilable source.
// @category TCERestoration
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;

public class ExportTceReference extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Expected output directory");
        Path dir = Paths.get(args[0], currentProgram.getName());
        Files.createDirectories(dir);
        DecompInterface decompiler = new DecompInterface();
        if (!decompiler.openProgram(currentProgram)) throw new IOException("Cannot open decompiler");
        int complete = 0, failed = 0;
        try (BufferedWriter index = Files.newBufferedWriter(dir.resolve("index.tsv"), StandardCharsets.UTF_8)) {
            index.write("address\tname\tstatus\tfile\n");
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext() && !monitor.isCancelled()) {
                Function f = functions.next();
                if (f.isExternal()) continue;
                String name = f.getName();
                String file = f.getEntryPoint() + "_" + name.replaceAll("[^A-Za-z0-9_.-]", "_") + ".c";
                DecompileResults result = decompiler.decompileFunction(f, 30, monitor);
                boolean ok = result.decompileCompleted() && result.getDecompiledFunction() != null;
                String header = "/* REFERENCE ONLY: " + currentProgram.getName() + " @ " + f.getEntryPoint() + " */\n";
                Files.writeString(dir.resolve(file), header + (ok ? result.getDecompiledFunction().getC() : "/* FAILED: " + result.getErrorMessage() + " */\n"), StandardCharsets.UTF_8);
                index.write(f.getEntryPoint() + "\t" + name + "\t" + (ok ? "decompiled" : "failed") + "\t" + file + "\n");
                if (ok) complete++; else failed++;
            }
        } finally { decompiler.dispose(); }
        println("TCE_EXPORT " + currentProgram.getName() + " complete=" + complete + " failed=" + failed);
    }
}
