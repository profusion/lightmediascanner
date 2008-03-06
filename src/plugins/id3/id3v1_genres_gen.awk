BEGIN {
  line=1;
  total_bytes=0;
}

{
  if (NF != 0) {
    sub(/[ \t]+$/, ""); # no trailing whitespace

    offsets[line] = total_bytes;
    genre[line] = $0;
    total_bytes += length($0) + 1;
    line++;
  }
}

END {
  print "static const unsigned ID3V1_NUM_GENRES = " (line - 1) ";";

  old_ORS=ORS;
  ORS="";
  print "static const char id3v1_genres_mem[] = \"";
  ORS="\\0";
  for (i = 1; i < line; i++)
    print genre[i];

  ORS=old_ORS;
  print "\";";

  ORS="";
  print "static const unsigned short id3v1_genres_offsets[] = {";
  for (i = 1; i < line; i++)
    print offsets[i] ", ";

  print total_bytes;

  ORS=old_ORS;
  print "};";
}
