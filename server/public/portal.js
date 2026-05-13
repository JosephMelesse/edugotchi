async function saveCategories() {
  const boxes   = Array.from(document.querySelectorAll('.cat-check'));
  const checked = boxes.filter(b => b.checked).map(b => b.value);

  if (checked.length === 0) {
    event.target.checked = true;
    event.target.closest('.cat-item').classList.add('cat-on');
    event.target.closest('.cat-item').classList.remove('cat-off');
    return;
  }

  boxes.forEach(b => {
    const item = b.closest('.cat-item');
    if (b.checked) { item.classList.add('cat-on');  item.classList.remove('cat-off'); }
    else            { item.classList.add('cat-off'); item.classList.remove('cat-on');  }
  });

  const st = document.getElementById('cat-status');
  st.textContent = 'Saving…';
  st.className   = 'cat-status';

  try {
    const res = await fetch('/settings', {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify({ enabledCategories: checked }),
    });
    st.textContent = res.ok ? 'Saved!' : 'Error saving. Please try again.';
    st.className   = 'cat-status ' + (res.ok ? 'saved' : 'error');
  } catch (e) {
    st.textContent = 'Could not reach server.';
    st.className   = 'cat-status error';
  }

  setTimeout(() => { st.textContent = ''; st.className = 'cat-status'; }, 2500);
}
