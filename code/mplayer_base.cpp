

 internal void
_debug_log(i32 flags, const char *file, int line, const char *format, ...)
{
  const char *name = "Info";
  if(flags & Log_Error)
  {
    name = "Error";
  }
  else if(flags & Log_Warning)
  {
    name = "Warning";
  }
  
  Memory_Checkpoint scratch = begin_scratch(0, 0);
  va_list args;
  va_start(args, format);
  String8 formated_log = str8_fv(scratch.arena, format, args);
  va_end(args);
  
  String8 output_string = str8_f(scratch.arena, "%s (%s:%i) %s\n", name, file, line, formated_log.cstr);
  
  // NOTE(fakhri): Log to stdout
  fprintf(stdout, "%s", output_string.cstr);
  
  
	#if OS_WINDOWS
		// NOTE(fakhri): Log to VS output, etc.
  {
    OutputDebugStringA(output_string.cstr);
    OutputDebugStringA("\n");
  }
	#endif
		end_scratch(scratch);
}
