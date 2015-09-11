(require 'gen-opts)

(defvar tgrep-options
   (make-options-parser 
    :name "tgrep" 
    :prelude 
    (an-c    
     (progn 
     ""
     "#define FREE_IOVEC_QUEUE_SIZE 8192"
     "#define MAX_SEARCH_TERM_LEN 1024"
     "#define IOVEC_BLOCK_SIZE 4096"
     "#define MAX_FILE_NAME 2048"
     ""
     (progn "enum path_type"
            (block "path_type_dir,path_type_file,path_type_stdin") ";"
            )) 0)
    :args 
    (list 
     (make-options-arg 
      :name "search_term"   :index 0
      :required t  :type 'string)
     (make-options-arg 
      :name "read_from"   :index 1
      :required t  :type 'string)
     (make-options-arg 
      :name "num_readers"  
      :short "r"  :long "num_readers"   
      :usage "Number of readers to run simultaneously" 
      :default "1"
      :required nil  :type 'int)
     (make-options-arg 
      :name "num_searchers"   
      :short "s"  :long "num_searchers"   
      :default "1"
      :usage "Number of searchers to run simultaneously" 
      :required nil  :type 'int)
     (make-options-arg 
      :name "debug"  
      :short "D"  :long "debug-level"   
      :usage "Debug verbosity" 
      :required nil  :type 'int)
     (make-options-arg 
      :name "iovec_block_size"  
      :short "b"  :long "block-size"   
      :usage "Block size for debug queue" 
      :required nil  :type 'int
      :default "IOVEC_BLOCK_SIZE")
     (make-options-arg 
      :name "iovec_queue_size"   
      :short "q"  :long "queue-size"   
      :usage "Queue for debug queue" 
      :required nil  :type 'int
      :default "FREE_IOVEC_QUEUE_SIZE")
     (make-options-arg 
      :name "path_type" 
      :type "enum path_type"
      :default "path_type_file"))))

(provide 'tgrep-arg-gen)
