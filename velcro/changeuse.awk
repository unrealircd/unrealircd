{
	if ($1 == "@use")
	{
		printf "#include \"%s.h\"\n", $2
	}
	else 
	{
		print
	}
}

