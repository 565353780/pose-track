function keypointsAll= inf2keypointsImg(expidx,parts,annolist_gt)

fprintf('inf2keypointsImg()\n');

p = rcnn_exp_params(expidx);
conf = rcnn_config('sub_dir', '/cachedir/test', 'exp_dir', [p.expDir '/' p.shortName]);
if (isfield(p,'infDir'))
    infDir = p.infDir;
else
    infDir = [conf.cache_dir '/inference'];    
end

keypointsAll = repmat(struct('imgname','','det',nan(16,3)), length(annolist_gt), 1);

for imgidx = 1:length(annolist_gt)
    
    fprintf('.');
    
    fnameInf = [infDir '/imgidx_' padZeros(num2str(imgidx-1),5)];
    load(fnameInf,'predAll');
    
    keypointsAll(imgidx).imgname = annolist_gt(imgidx).image.name;
    assert(length(p.pidxs) == length(predAll));
    
    for i = 1:length(p.pidxs)
        pidx = p.pidxs(i);
        % part is a joint
        assert(parts(pidx+1).pos(1) == parts(pidx+1).pos(2));
        jidx = parts(pidx+1).pos(1);
        
        det = predAll{i};
        idxs = find(det(:,4)==1);
        [val,id] = max(det(idxs,3));%det(:,4)==1
        x = det(idxs(id),1);
        y = det(idxs(id),2);
        keypointsAll(imgidx).det(jidx+1,:) = [[x y] val];
    end
    if (~mod(imgidx, 100))
        fprintf(' %d/%d\n',imgidx,length(keypointsAll));
    end
end
fprintf(' done\n');

end