

function isalpha(s){
  return /^[a-z]+$/i.test(s);
}

function isdigit(s){
  return /^\d+$/.test(s);
}

var ckeyword = {
  "auto": 0, "break": 0,
  "case": 0, "char": 0, "const": 0, "continue": 0,
  "default": 0, "do": 0, "double": 0, 
  "else": 0, "enum": 0, "extern": 0,
  "float": 0, "for": 0, "goto": 0,
  "if": 0, "inline": 0, "int": 0, "long": 0,
  "register": 0, "restrict": 0, "return": 0, "short": 0, "signed": 0,
  "sizeof": 0, "static": 0, "struct": 0, "switch": 0,
  "typedef": 0, "union": 0, "unsigned": 0,
  "void": 0, "volatile": 0, "while": 0
};

function csyntax(s){
  var id,s2,st,c;
  s2="";
  var i=0;
  while(i<s.length){
    c=s[i];
    if(isalpha(c) || s[i]=='_'){
      id="";
      while(i<s.length && (isalpha(s[i]) || isdigit(s[i]) || s[i]=='_')){
        id+=s[i];
        i++;
      }
      if(ckeyword.hasOwnProperty(id)){
        s2+="<span class='keyword'>"+id+"</span>";
      }else{
        s2+=id;
      }
    }else if(isdigit(c)){
      st="";
      while(i<s.length && isdigit(s[i])){
        st+=s[i];
        i++;
      }
      s2+="<span class='number'>"+st+"</span>";
    }else if(c=='"'){
      st="<span class='string'>\"";
      i++;
      while(i<s.length && s[i]!='"'){
        st+=s[i];
        i++;
      }
      i++;
      st+="\"</span>";
      s2+=st;
    }else if(c=="'"){
      st="<span class='string'>'";
      i++;
      while(i<s.length && s[i]!="'"){
        st+=s[i];
        i++;
      }
      i++;
      st+="'</span>";
      s2+=st;
    }else if(c=='#'){
      st="<span class='preprocessor'>";
      while(i<s.length && s[i]!='\n'){
        st+=s[i];
        i++;
      }
      st+="</span>";
      s2+=st;
    }else if(c=='/' && i+1<s.length && s[i+1]=='/'){
      st="<span class='comment'>";
      while(i<s.length && s[i]!='\n'){
        st+=s[i];
        i++;
      }
      st+="</span>";
      s2+=st;
    }else if(c=='/' && i+1<s.length && s[i+1]=='*'){
      st="<span class='comment'>/*";
      i+=2;
      while(i+1<s.length && s[i]!='*' && s[i+1]!='/'){
        st+=s[i];
        i++;
      }
      i+=2;
      st+="*/</span>";
      s2+=st;
    }else if(c=='&'){
      st="";
      while(i<s.length && s[i]!=';'){
        st+=s[i];
        i++;
      }
      s2+="<span class='symbol'>"+st+";</span>";
      i++;
    }else if(c=='(' || c==')' || c=='[' || c==']' || c=='{' || c=='}'){
      s2+="<span class='bracket'>"+c+"</span>";
      i++;
    }else if(c=='+' || c=='-' || c=='*' || c=='/' || c=='|' ||
      c=='.' || c=='=' || c=='!' || c==':' || c=='%' || c=='^' ||
      c=='~'
    ){
      s2+="<span class='symbol'>"+c+"</span>";
      i++;
    }else{
      s2+=c;
      i++;
    }
  }
  return s2;
}

function main(){
  var a = document.getElementsByClassName("c");
  for(var i=0; i<a.length; i++){
    a[i].innerHTML = csyntax(a[i].innerHTML);
  }
}

window.onload = main;
