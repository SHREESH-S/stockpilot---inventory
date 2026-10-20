const $ = (s) => document.querySelector(s);
const $$ = (s) => [...document.querySelectorAll(s)];
let products = [], orders = [], editingId = null, stockMode = "in";
const money = n => new Intl.NumberFormat("en-IN",{style:"currency",currency:"INR",maximumFractionDigits:2}).format(Number(n)||0);
const esc = s => String(s ?? "").replace(/[&<>"']/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
const stockStatus = p => p.quantity === 0 ? ["Out of stock","out"] : p.quantity <= p.threshold ? ["Low stock","low"] : ["In stock","healthy"];
async function api(url, options={}) {
  const res = await fetch(url,{headers:{"Content-Type":"application/json"},...options});
  let data={}; try{data=await res.json()}catch{}
  if(!res.ok) throw new Error(data.error || `Request failed (${res.status})`);
  return data;
}
function toast(message,type="success"){const el=document.createElement("div");el.className=`toast ${type}`;el.textContent=message;$("#toastRegion").append(el);setTimeout(()=>el.remove(),3200)}
function initials(name){return String(name||"?").split(/\s+/).slice(0,2).map(x=>x[0]||"").join("").toUpperCase()}
function productCell(p){return `<div class="product-cell"><div class="product-avatar">${esc(initials(p.name))}</div><div><div class="product-name">${esc(p.name)}</div><div class="product-category">${esc(p.category||"General")}</div></div></div>`}
function statusBadge(p){const [label,cls]=stockStatus(p);return `<span class="status ${cls}">${label}</span>`}
function renderOverview(){
  const low=products.filter(p=>p.quantity<=p.threshold);
  $("#statProducts").textContent=products.length;
  $("#statUnits").textContent=products.reduce((a,p)=>a+p.quantity,0).toLocaleString("en-IN");
  $("#statValue").textContent=money(products.reduce((a,p)=>a+p.quantity*p.price,0));
  $("#statLow").textContent=low.length;
  $("#navProductCount").textContent=products.length;
  $("#navOrderCount").textContent=orders.length;
  $("#alertCount").textContent=low.length;
  $("#overviewTable").innerHTML=products.slice(0,5).map(p=>`<tr><td>${productCell(p)}</td><td class="id-text">#${p.id}</td><td class="quantity-cell">${p.quantity}</td><td>${money(p.price)}</td><td>${statusBadge(p)}</td></tr>`).join("");
  $("#overviewEmpty").classList.toggle("hidden",products.length>0);
  $("#overviewTable").closest(".table-wrap").classList.toggle("hidden",products.length===0);
  $("#overviewAlerts").innerHTML=low.slice(0,5).map(p=>`<div class="alert-item"><div class="alert-product-icon">!</div><div class="alert-item-main"><strong>${esc(p.name)}</strong><small>Minimum ${p.threshold} units · ID #${p.id}</small></div><span class="alert-qty">${p.quantity} left</span></div>`).join("");
  $("#alertsClear").classList.toggle("hidden",low.length>0);
}
function renderProducts(){
  const q=$("#searchInput").value.trim().toLowerCase();
  const shown=products.filter(p=>`${p.id} ${p.name} ${p.category}`.toLowerCase().includes(q));
  $("#productsTable").innerHTML=shown.map(p=>`<tr><td>${productCell(p)}</td><td class="id-text">#${p.id}</td><td>${esc(p.category||"General")}</td><td class="quantity-cell">${p.quantity}</td><td>${money(p.price)}</td><td>${statusBadge(p)}</td><td><div class="table-actions"><button class="table-action" data-edit="${p.id}">Edit</button><button class="table-action delete" data-delete="${p.id}">Delete</button></div></td></tr>`).join("");
  $("#productTableCount").textContent=shown.length;
  $("#productsEmpty").classList.toggle("hidden",shown.length>0);
  $("#productsTable").closest(".table-wrap").classList.toggle("hidden",shown.length===0);
}
function renderOrders(){
  $("#ordersTable").innerHTML=orders.map((o,i)=>`<tr><td><strong>#${o.orderId}</strong></td><td>${esc(o.productName)}</td><td class="id-text">#${o.productId}</td><td class="quantity-cell">${o.quantity}</td><td><span class="status ${i===0?"low":"healthy"}">${i===0?"Next to process":`Position ${i+1}`}</span></td></tr>`).join("");
  $("#orderCount").textContent=orders.length;
  $("#ordersEmpty").classList.toggle("hidden",orders.length>0);
  $("#ordersTable").closest(".table-wrap").classList.toggle("hidden",orders.length===0);
  $("#processOrderBtn").disabled=orders.length===0;
  $("#processOrderBtn").style.opacity=orders.length===0?".5":"1";
}
function renderAlerts(){
  const low=products.filter(p=>p.quantity<=p.threshold);
  $("#alertsTable").innerHTML=low.map(p=>`<tr><td>${productCell(p)}</td><td class="id-text">#${p.id}</td><td class="quantity-cell">${p.quantity}</td><td>${p.threshold}</td><td><span class="status low">${Math.max(0,p.threshold-p.quantity)} units below</span></td><td><button class="table-action" data-stockin="${p.id}">Restock</button></td></tr>`).join("");
  $("#alertsPageEmpty").classList.toggle("hidden",low.length>0);
  $("#alertsTable").closest(".table-wrap").classList.toggle("hidden",low.length===0);
  $("#alertSummaryTitle").textContent=low.length?`${low.length} product${low.length===1?"":"s"} need attention`:"No stock alerts";
  $("#alertSummaryText").textContent=low.length?"Review the items below and record incoming stock to bring them above their thresholds.":"Your inventory is within its configured thresholds.";
}
function fillProductSelects(){
  const opts=products.map(p=>`<option value="${p.id}">${esc(p.name)} — #${p.id} (${p.quantity} available)</option>`).join("");
  $("#stockProduct").innerHTML=opts;
  $("#orderProduct").innerHTML=opts;
}
async function refresh(){
  try{
    const [p,o]=await Promise.all([api(`/api/products?sort=${encodeURIComponent($("#sortSelect")?.value||"id")}`),api("/api/orders")]);
    products=p;orders=o;renderOverview();renderProducts();renderOrders();renderAlerts();fillProductSelects();
  }catch(e){toast(`Could not connect to C backend: ${e.message}`,"error")}
}
function showView(view){
  $$(".view").forEach(v=>v.classList.toggle("active",v.id===`view-${view}`));
  $$(".nav-item").forEach(b=>b.classList.toggle("active",b.dataset.view===view));
  $("#crumb").textContent=({overview:"Overview",products:"Products",orders:"Order queue",alerts:"Stock alerts"})[view]||"Overview";
  $("#sidebar").classList.remove("open");
}
function openProductModal(p=null){
  editingId=p?p.id:null;$("#productForm").reset();$("#productError").classList.add("hidden");
  $("#productModalTitle").textContent=p?"Edit product":"Add a product";
  $("#productId").disabled=!!p;
  $("#productId").value=p?p.id:"";
  $("#productName").value=p?p.name:"";
  $("#productCategory").value=p?p.category:"";
  $("#productPrice").value=p?p.price:"";
  $("#productQuantity").value=p?p.quantity:"";
  $("#productThreshold").value=p?p.threshold:5;
  $("#productModal").classList.remove("hidden");
  setTimeout(()=>$("#productName").focus(),50);
}
function closeProductModal(){$("#productModal").classList.add("hidden");editingId=null}
function openStock(mode="in",id=null){
  if(!products.length){toast("Add a product before recording stock movement.","error");return}
  stockMode=mode;$("#stockTitle").textContent=mode==="in"?"Stock in":"Stock out";
  $("#stockDescription").textContent=mode==="in"?"Record incoming inventory.":"Record stock issued or sold.";
  $("#stockEyebrow").textContent=mode==="in"?"INCOMING INVENTORY":"OUTGOING INVENTORY";
  fillProductSelects();if(id)$("#stockProduct").value=id;
  $("#stockQuantity").value="";$("#stockError").classList.add("hidden");$("#stockModal").classList.remove("hidden");
}
function openOrder(){
  if(!products.length){toast("Add a product before creating an order.","error");return}
  fillProductSelects();$("#orderQuantity").value="";$("#orderError").classList.add("hidden");$("#orderModal").classList.remove("hidden");
}
function showError(el,msg){el.textContent=msg;el.classList.remove("hidden")}
$$("[data-view]").forEach(b=>b.addEventListener("click",()=>showView(b.dataset.view)));
$$("[data-goto]").forEach(b=>b.addEventListener("click",()=>showView(b.dataset.goto)));
$$("[data-open-modal]").forEach(b=>b.addEventListener("click",()=>openProductModal()));
$("#topAddBtn").addEventListener("click",()=>openProductModal());
$("#mobileMenu").addEventListener("click",()=>$("#sidebar").classList.toggle("open"));
$("#refreshBtn").addEventListener("click",refresh);
$("#searchInput").addEventListener("input",renderProducts);
$("#sortSelect").addEventListener("change",refresh);
$$("[data-close-modal]").forEach(b=>b.addEventListener("click",closeProductModal));
$("#productModal").addEventListener("click",e=>{if(e.target.id==="productModal")closeProductModal()});
$("#productForm").addEventListener("submit",async e=>{
  e.preventDefault();const data={id:Number($("#productId").value),name:$("#productName").value.trim(),category:$("#productCategory").value.trim()||"General",price:Number($("#productPrice").value),quantity:Number($("#productQuantity").value),threshold:Number($("#productThreshold").value)};
  try{await api("/api/products",{method:editingId?"PUT":"POST",body:JSON.stringify(data)});closeProductModal();await refresh();toast(editingId?"Product updated.":"Product added successfully.");}
  catch(err){showError($("#productError"),err.message)}
});
$("#productsTable").addEventListener("click",async e=>{
  const edit=e.target.closest("[data-edit]"),del=e.target.closest("[data-delete]");
  if(edit){const p=products.find(x=>x.id===Number(edit.dataset.edit));if(p)openProductModal(p)}
  if(del){const id=Number(del.dataset.delete),p=products.find(x=>x.id===id);if(!confirm(`Delete "${p?.name||id}"? This cannot be undone.`))return;try{await api(`/api/products?id=${id}`,{method:"DELETE"});await refresh();toast("Product deleted.");}catch(err){toast(err.message,"error")}}
});
$("#quickStockIn").addEventListener("click",()=>openStock("in"));
$("#stockModal").querySelectorAll("[data-close-stock]").forEach(b=>b.addEventListener("click",()=>$("#stockModal").classList.add("hidden")));
$("#stockForm").addEventListener("submit",async e=>{
  e.preventDefault();const data={id:Number($("#stockProduct").value),quantity:Number($("#stockQuantity").value)};
  try{await api(`/api/stock-${stockMode}`,{method:"POST",body:JSON.stringify(data)});$("#stockModal").classList.add("hidden");await refresh();toast(stockMode==="in"?"Stock-in recorded.":"Stock-out recorded.");}
  catch(err){showError($("#stockError"),err.message)}
});
$("#quickOrder").addEventListener("click",openOrder);$("#newOrderBtn").addEventListener("click",openOrder);$("#emptyOrderBtn").addEventListener("click",openOrder);
$("#orderModal").querySelectorAll("[data-close-order]").forEach(b=>b.addEventListener("click",()=>$("#orderModal").classList.add("hidden")));
$("#orderForm").addEventListener("submit",async e=>{
  e.preventDefault();const data={productId:Number($("#orderProduct").value),quantity:Number($("#orderQuantity").value)};
  try{await api("/api/orders",{method:"POST",body:JSON.stringify(data)});$("#orderModal").classList.add("hidden");await refresh();toast("Order added to the FIFO queue.");}
  catch(err){showError($("#orderError"),err.message)}
});
$("#processOrderBtn").addEventListener("click",async()=>{
  if(!orders.length)return;
  try{await api("/api/orders/process",{method:"POST",body:"{}"});await refresh();toast("Oldest order processed; stock updated.");}
  catch(err){toast(err.message,"error")}
});
$("#alertsTable").addEventListener("click",e=>{const b=e.target.closest("[data-stockin]");if(b)openStock("in",Number(b.dataset.stockin))});
$("#todayDate").textContent=new Intl.DateTimeFormat("en-IN",{weekday:"short",day:"2-digit",month:"short",year:"numeric"}).format(new Date()).toUpperCase();
document.addEventListener("keydown",e=>{if(e.key==="Escape"){closeProductModal();$("#stockModal").classList.add("hidden");$("#orderModal").classList.add("hidden")}});
refresh();